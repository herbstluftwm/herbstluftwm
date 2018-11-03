/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "key.h"
#include "globals.h"
#include "utils.h"
#include "ipc-protocol.h"
#include "command.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <regex.h>
#include "glib-backports.h"
#include <X11/keysym.h>
#include <X11/XKBlib.h>

static unsigned int numlockmask = 0;
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask))

unsigned int* get_numlockmask_ptr() {
    return &numlockmask;
}

static GList* g_key_binds = nullptr;

void key_init() {
    update_numlockmask();
}

void key_destroy() {
    key_remove_all_binds();
}

void key_remove_all_binds() {
    g_list_free_full(g_key_binds, (GDestroyNotify)keybinding_free);
    g_key_binds = nullptr;
    regrab_keys();
}

void keybinding_free(KeyBinding* binding) {
    argv_free(binding->cmd_argc, binding->cmd_argv);
    g_free(binding);
}

typedef struct {
    const char* name;
    unsigned int mask;
} Name2Modifier;

static Name2Modifier g_modifier_names[] = {
    { "Mod1",       Mod1Mask },
    { "Mod2",       Mod2Mask },
    { "Mod3",       Mod3Mask },
    { "Mod4",       Mod4Mask },
    { "Mod5",       Mod5Mask },
    { "Alt",        Mod1Mask },
    { "Super",      Mod4Mask },
    { "Shift",      ShiftMask },
    { "Control",    ControlMask },
    { "Ctrl",       ControlMask },
};

unsigned int modifiername2mask(const char* name) {
    Name2Modifier* elem;
    elem = STATIC_TABLE_FIND_STR(Name2Modifier, g_modifier_names, name,
                                 (char*)name);
    return elem ? elem->mask : 0;
}

/**
 * \brief finds a (any) modifier in mask and returns its name
 *
 * \return  the name as a char string. You must not free it.
 */
const char* modifiermask2name(unsigned int mask) {
    for (int i = 0; i < LENGTH(g_modifier_names); i++) {
        if (g_modifier_names[i].mask & mask) {
            return g_modifier_names[i].name;
        }
    }
    return nullptr;
}

int keybind(int argc, char** argv, Output output) {
    if (argc <= 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    KeyBinding new_bind;
    // get keycode
    if (!string2key(argv[1], &(new_bind.modifiers), &(new_bind.keysym))) {
        output << argv[0] << ": No such KeySym/modifier\n";
        return HERBST_INVALID_ARGUMENT;
    }
    KeyCode keycode = XKeysymToKeycode(g_display, new_bind.keysym);
    if (!keycode) {
        output << argv[0] << ": No keycode for symbol "
               << XKeysymToString(new_bind.keysym) << std::endl;
        return HERBST_INVALID_ARGUMENT;
    }
    // remove existing binding with same keysym/modifiers
    key_remove_bind_with_keysym(new_bind.modifiers, new_bind.keysym);
    // create a copy of the command to execute on this key
    new_bind.cmd_argc = argc - 2;
    new_bind.cmd_argv = argv_duplicate(new_bind.cmd_argc, argv+2);
    // add keybinding
    KeyBinding* data = g_new(KeyBinding, 1);
    *data = new_bind;
    g_key_binds = g_list_append(g_key_binds, data);
    // grab for events on this keycode
    grab_keybind(data, nullptr);
    return 0;
}

bool string2modifiers(char* string, unsigned int* modmask) {
    // example strings: "Mod1-space" "Mod4+f" "f"
    // split string at "+-"
    char** splitted = g_strsplit_set(string, KEY_COMBI_SEPARATORS, 0);
    // this should give at least one part
    if (!splitted[0]) {
        fprintf(stderr, "warning: empty keysym\n");
        g_strfreev(splitted);
        return false;
    }
    // all parts except last one are modifiers
    int i;
    *modmask = 0;
    for (i = 0; splitted[i+1] != nullptr; i++) {
        // while the i'th element is not the last part
        unsigned int cur_mask = modifiername2mask(splitted[i]);
        if (cur_mask == 0) {
            fprintf(stderr, "warning: unknown Modifier \"%s\"\n", splitted[i]);
            g_strfreev(splitted);
            return false;
        }
        *modmask |= cur_mask;
    }
    // splitted string is not needed anymore
    g_strfreev(splitted);
    return true;
}

bool string2key(char* string, unsigned int* modmask, KeySym* keysym) {
    if (!string2modifiers(string, modmask)) {
        return false;
    }
    // last one is the key
    const char* last_token = strlasttoken(string, KEY_COMBI_SEPARATORS);
    *keysym = XStringToKeysym(last_token);
    if (*keysym == NoSymbol) {
        fprintf(stderr, "warning: unknown KeySym \"%s\"\n", last_token);
        return false;
    }
    return true;
}

static gint keysym_equals(const KeyBinding* a, const KeyBinding* b) {
    bool equal = (CLEANMASK(a->modifiers) == CLEANMASK(b->modifiers));
    equal = equal && (a->keysym == b->keysym);
    return equal ? 0 : -1;
}

void handle_key_press(XEvent* ev) {
    KeyBinding pressed;
    pressed.keysym = XkbKeycodeToKeysym(g_display, ev->xkey.keycode, 0, 0);
    pressed.modifiers = ev->xkey.state;
    GList* element = g_list_find_custom(g_key_binds, &pressed, (GCompareFunc)keysym_equals);
    if (element && element->data) {
        KeyBinding* found = (KeyBinding*)element->data;
        // duplicate the args in the case this keybinding removes itself
        char** argv =  argv_duplicate(found->cmd_argc, found->cmd_argv);
        int argc = found->cmd_argc;
        // call the command
        call_command_no_output(argc, argv);
        argv_free(argc, argv);
    }
}

int keyunbind(int argc, char** argv, Output output) {
    if (argc <= 1) {
        return HERBST_NEED_MORE_ARGS;
    }
    // remove all keybinds if wanted
    if (!strcmp(argv[1], "-F") || !strcmp(argv[1], "--all")) {
        key_remove_all_binds();
        return 0;
    }
    unsigned int modifiers;
    KeySym keysym;
    // get keycode
    if (!string2key(argv[1], &modifiers, &keysym)) {
        output << argv[0] << ": No such KeySym/modifier\n";
        return HERBST_INVALID_ARGUMENT;
    }
    if (key_remove_bind_with_keysym(modifiers, keysym) == false) {
        output << argv[0] << ": Key \"" << argv[1] << "\" is not bound\n";
    }
    regrab_keys();
    return 0;
}

bool key_remove_bind_with_keysym(unsigned int modifiers, KeySym keysym){
    KeyBinding bind;
    bind.modifiers = modifiers;
    bind.keysym = keysym;
    // search this keysym in list and remove it
    GList* element = g_list_find_custom(g_key_binds, &bind, (GCompareFunc)keysym_equals);
    if (!element) {
        return false;
    }
    KeyBinding* data = (KeyBinding*)element->data;
    keybinding_free(data);
    g_key_binds = g_list_remove_link(g_key_binds, element);
    g_list_free_1(element);
    return true;
}

void regrab_keys() {
    update_numlockmask();
    // init modifiers after updating numlockmask
    XUngrabKey(g_display, AnyKey, AnyModifier, g_root); // remove all current grabs
    g_list_foreach(g_key_binds, (GFunc)grab_keybind, nullptr);
}

void grab_keybind(KeyBinding* binding, void* useless_pointer) {
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    KeyCode keycode = XKeysymToKeycode(g_display, binding->keysym);
    if (!keycode) {
        // ignore unknown keysyms
        return;
    }
    // grab key for each modifier that is ignored (capslock, numlock)
    for (int i = 0; i < LENGTH(modifiers); i++) {
        XGrabKey(g_display, keycode, modifiers[i]|binding->modifiers, g_root,
                 True, GrabModeAsync, GrabModeAsync);
    }
    // mark the keybinding as enabled
    binding->enabled = true;
}

void ungrab_keybind(KeyBinding* binding, void* useless_pointer) {
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    KeyCode keycode = XKeysymToKeycode(g_display, binding->keysym);
    if (!keycode) {
        // ignore unknown keysyms
        return;
    }
    // grab key for each modifier that is ignored (capslock, numlock)
    for (int i = 0; i < LENGTH(modifiers); i++) {
        XUngrabKey(g_display, keycode, modifiers[i]|binding->modifiers, g_root);
    }
    binding->enabled = false;
}


// update the numlockmask
// from dwm.c
void update_numlockmask() {
    unsigned int i, j;
    XModifierKeymap *modmap;

    numlockmask = 0;
    modmap = XGetModifierMapping(g_display);
    for(i = 0; i < 8; i++)
        for(j = 0; j < modmap->max_keypermod; j++)
            if(modmap->modifiermap[i * modmap->max_keypermod + j]
               == XKeysymToKeycode(g_display, XK_Num_Lock))
                numlockmask = (1 << i);
    XFreeModifiermap(modmap);
}

/**
 * \brief   converts the given binding to a GString
 * \return  the new created GString. You have to destroy it.
 */
GString* keybinding_to_g_string(KeyBinding* binding) {
    GString* str = g_string_new("");

    /* add modifiers */
    unsigned int old_mask = 0, new_mask = binding->modifiers;
    while (new_mask != 0 && new_mask != old_mask) {
        old_mask = new_mask;

        /* try to find a modifier */
        const char* name = modifiermask2name(old_mask);
        if (!name) {
            break;
        }
        g_string_append(str, name);
        g_string_append_c(str, KEY_COMBI_SEPARATORS[0]);
        /* remove found mask from mask */
        new_mask = old_mask & ~ modifiername2mask(name);
    }

    /* add keysym */
    const char* name = XKeysymToString(binding->keysym);
    if (!name) {
        g_warning("XKeysymToString failed! using \'?\' instead\n");
        name = "?";
    }
    g_string_append(str, name);

    return str;
}

// STRTODO
struct key_find_context {
    Output      output;
    const char* needle;
    size_t      needle_len;
};

static void key_find_binds_helper(KeyBinding* b, struct key_find_context* c) {
    GString* name = keybinding_to_g_string(b);
    if (!strncmp(c->needle, name->str, c->needle_len)) {
        /* add to output if key starts with searched needle */
        c->output << name->str << std::endl;
    }
    g_string_free(name, true);
}

void key_find_binds(const char* needle, Output output) {
    struct key_find_context c = {
        output, needle, strlen(needle)
    };
    g_list_foreach(g_key_binds, (GFunc)key_find_binds_helper, &c);
}

static void key_list_binds_helper(KeyBinding* b, std::ostream* ptr_output) {
    Output output = *ptr_output;
    // add keybinding
    GString* name = keybinding_to_g_string(b);
    output << name->str;
    g_string_free(name, true);
    // add associated command
    for (int i = 0; i < b->cmd_argc; i++) {
        output << "\t" << b->cmd_argv[i];
    }
    output << "\n";
}

int key_list_binds(int argc, char** argv, Output output) {
    g_list_foreach(g_key_binds, (GFunc)key_list_binds_helper, &output);
    return 0;
}

void complete_against_keysyms(const char* needle, char* prefix, Output output) {
    // get all possible keysyms
    int min, max;
    XDisplayKeycodes(g_display, &min, &max);
    int kc_count = max - min + 1;
    int ks_per_kc; // count of keysysms per keycode
    KeySym* keysyms;
    keysyms = XGetKeyboardMapping(g_display, min, kc_count, &ks_per_kc);
    // only symbols at a position i*ks_per_kc are symbols that are recieved in
    // a keyevent, it should be the symbol for the keycode if no modifier is
    // pressed
    for (int i = 0; i < kc_count; i++) {
        if (keysyms[i * ks_per_kc] != NoSymbol) {
            char* str = XKeysymToString(keysyms[i * ks_per_kc]);
            try_complete_prefix(needle, str, prefix, output);
        }
    }
    XFree(keysyms);
}

void complete_against_modifiers(const char* needle, char seperator,
                                char* prefix, Output output) {
    GString* buf = g_string_sized_new(20);
    for (int i = 0; i < LENGTH(g_modifier_names); i++) {
        g_string_printf(buf, "%s%c", g_modifier_names[i].name, seperator);
        try_complete_prefix_partial(needle, buf->str, prefix, output);
    }
    g_string_free(buf, true);
}

static void key_set_keymask_helper(KeyBinding* b, regex_t *keymask_regex) {
    // add keybinding
    bool enabled = true;
    if (keymask_regex) {
        GString* name = keybinding_to_g_string(b);
        regmatch_t match;
        int status = regexec(keymask_regex, name->str, 1, &match, 0);
        // only accept it, if it matches the entire string
        if (status == 0
            && match.rm_so == 0
            && match.rm_eo == strlen(name->str)) {
            enabled = true;
        } else {
            // Keybinding did not match, therefore we disable it
            enabled = false;
        }
        g_string_free(name, true);
    }

    if (enabled && !b->enabled) {
        grab_keybind(b, nullptr);
    } else if(!enabled && b->enabled) {
        ungrab_keybind(b, nullptr);
    }
}

void key_set_keymask(HSTag *tag, HSClient *client) {
    regex_t     keymask_regex;
    if (client && client->keymask_ != "") {
        int status = regcomp(&keymask_regex, ((std::string)client->keymask_).c_str(), REG_EXTENDED);
        if (status == 0) {
            g_list_foreach(g_key_binds, (GFunc)key_set_keymask_helper,
                           &keymask_regex);
            return;
        } else {
            char buf[ERROR_STRING_BUF_SIZE];
            regerror(status, &keymask_regex, buf, ERROR_STRING_BUF_SIZE);
            HSDebug("keymask: Can not parse regex \"%s\" from keymask: %s",
                    ((std::string)client->keymask_).c_str(), buf);
        }
    }
    // Enable all keys again
    g_list_foreach(g_key_binds, (GFunc)key_set_keymask_helper, 0);
}
