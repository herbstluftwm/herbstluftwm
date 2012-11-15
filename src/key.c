/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "key.h"
#include "globals.h"
#include "utils.h"
#include "ipc-protocol.h"
#include "command.h"

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

static unsigned int numlockmask = 0;
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask))

unsigned int* get_numlockmask_ptr() {
    return &numlockmask;
}

GList* g_key_binds = NULL;

void key_init() {
    update_numlockmask();
}

void key_destroy() {
    key_remove_all_binds();
}

void key_remove_all_binds() {
#if GLIB_CHECK_VERSION(2, 28, 0)
    // only available since glib 2.28
    g_list_free_full(g_key_binds, (GDestroyNotify)keybinding_free);
#else
    // actually this is not c-standard-compatible because of casting
    // an one-parameter-function to an 2-parameter-function.
    // but it should work on almost all architectures (maybe not amd64?)
    g_list_foreach(g_key_binds, (GFunc)keybinding_free, 0);
    g_list_free(g_key_binds);
#endif
    g_key_binds = NULL;
    regrab_keys();
}

void keybinding_free(KeyBinding* binding) {
    argv_free(binding->cmd_argc, binding->cmd_argv);
    g_free(binding);
}

typedef struct {
    char* name;
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
char*   modifiermask2name(unsigned int mask) {
    int i;
    for (i = 0; i < LENGTH(g_modifier_names); i++) {
        if (g_modifier_names[i].mask & mask) {
            return g_modifier_names[i].name;
        }
    }
    return NULL;
}

int keybind(int argc, char** argv, GString* output) {
    if (argc <= 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    KeyBinding new_bind;
    // get keycode
    if (!string2key(argv[1], &(new_bind.modifiers), &(new_bind.keysym))) {
        g_string_append_printf(output,
            "%s: No such KeySym/modifier!\n", argv[0]);
        return HERBST_INVALID_ARGUMENT;
    }
    KeyCode keycode = XKeysymToKeycode(g_display, new_bind.keysym);
    if (!keycode) {
        g_string_append_printf(output,
            "%s: no keycode for symbol %s\n",
            argv[0], XKeysymToString(new_bind.keysym));
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
    grab_keybind(data, NULL);
    return 0;
}

bool string2modifiers(char* string, unsigned int* modmask) {
    // example strings: "Mod1-space" "Mod4+f" "f"
    // split string at "+-"
    char** splitted = g_strsplit_set(string, KEY_COMBI_SEPARATORS, 0);
    // this should give at least one part
    if (NULL == splitted[0]) {
        fprintf(stderr, "warning: empty keysym\n");
        g_strfreev(splitted);
        return false;
    }
    // all parts except last one are modifiers
    int i;
    *modmask = 0;
    for (i = 0; splitted[i+1] != NULL; i++) {
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
    char* last_token = strlasttoken(string, KEY_COMBI_SEPARATORS);
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

int keyunbind(int argc, char** argv, GString* output) {
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
        g_string_append_printf(output,
            "%s: No such KeySym/modifier!\n", argv[0]);
        return HERBST_INVALID_ARGUMENT;
    }
    key_remove_bind_with_keysym(modifiers, keysym);
    regrab_keys();
    return 0;
}

void key_remove_bind_with_keysym(unsigned int modifiers, KeySym keysym){
    KeyBinding bind;
    bind.modifiers = modifiers;
    bind.keysym = keysym;
    // search this keysym in list and remove it
    GList* element = g_list_find_custom(g_key_binds, &bind, (GCompareFunc)keysym_equals);
    if (!element) {
        return;
    }
    KeyBinding* data = element->data;
    keybinding_free(data);
    g_key_binds = g_list_remove_link(g_key_binds, element);
    g_list_free_1(element);
}

void regrab_keys() {
    update_numlockmask();
    // init modifiers after updating numlockmask
    XUngrabKey(g_display, AnyKey, AnyModifier, g_root); // remove all current grabs
    g_list_foreach(g_key_binds, (GFunc)grab_keybind, NULL);
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
        char* name = modifiermask2name(old_mask);
        if (!name) {
            break;
        }
        g_string_append(str, name);
        g_string_append_c(str, KEY_COMBI_SEPARATORS[0]);
        /* remove found mask from mask */
        new_mask = old_mask & ~ modifiername2mask(name);
    }

    /* add keysym */
    char* name = XKeysymToString(binding->keysym);
    if (!name) {
        g_warning("XKeysymToString failed! using \'?\' instead\n");
        name = "?";
    }
    g_string_append(str, name);

    return str;
}

struct key_find_context {
    GString*   output;
    char*       needle;
    size_t      needle_len;
};

static void key_find_binds_helper(KeyBinding* b, struct key_find_context* c) {
    GString* name = keybinding_to_g_string(b);
    if (!strncmp(c->needle, name->str, c->needle_len)) {
        /* add to output if key starts with searched needle */
        g_string_append(c->output, name->str);
        g_string_append_c(c->output, '\n');
    }
    g_string_free(name, true);
}

void key_find_binds(char* needle, GString* output) {
    struct key_find_context c = {
        output, needle, strlen(needle)
    };
    g_list_foreach(g_key_binds, (GFunc)key_find_binds_helper, &c);
}

static void key_list_binds_helper(KeyBinding* b, GString* output) {
    // add keybinding
    GString* name = keybinding_to_g_string(b);
    g_string_append(output, name->str);
    g_string_free(name, true);
    // add associated command
    for (int i = 0; i < b->cmd_argc; i++) {
        g_string_append_c(output, '\t');
        g_string_append(output, b->cmd_argv[i]);
    }
    g_string_append_c(output, '\n');
}

int key_list_binds(int argc, char** argv, GString* output) {
    g_list_foreach(g_key_binds, (GFunc)key_list_binds_helper, output);
    return 0;
}

