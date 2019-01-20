#include "key.h"

#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <regex.h>
#include <string>

#include "client.h"
#include "command.h"
#include "glib-backports.h"
#include "globals.h"
#include "ipc-protocol.h"
#include "utils.h"

using std::unique_ptr;
using std::vector;

static unsigned int numlockmask = 0;
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask))

unsigned int* get_numlockmask_ptr() {
    return &numlockmask;
}

// TODO: Turn this into a private member of KeyManager as soon as possible.
vector<unique_ptr<KeyBinding>> g_key_binds = {};

void key_init() {
    update_numlockmask();
}

void key_destroy() {
    key_remove_all_binds();
}

void key_remove_all_binds() {
    g_key_binds.clear();
    regrab_keys();
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

vector<std::string> splitKeySpec(std::string keySpec)
{
    // Normalize spec to use a single separator:
    char baseSep = KEY_COMBI_SEPARATORS[0];
    for (auto &sep : KEY_COMBI_SEPARATORS) {
        std::replace(keySpec.begin(), keySpec.end(), sep, baseSep);
    }

    // Split spec into tokens:
    vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(keySpec);
    while (std::getline(tokenStream, token, baseSep)) {
        tokens.push_back(token);
    }
    return tokens;
}

bool string2modifiers(const std::string& str, unsigned int* modmask) {
    // example strings: "Mod1-space" "Mod4+f" "f"
    auto splitted = splitKeySpec(str);
    // this should give at least one part
    if (splitted.empty()) {
        HSWarning("empty keysym\n");
        return false;
    }
    // all parts except last one are modifiers
    *modmask = 0;
    for (auto iter = splitted.begin(); iter + 1 != splitted.end(); iter++) {
        unsigned int cur_mask = modifiername2mask(iter->c_str());
        if (cur_mask == 0) {
            HSWarning("unknown modifier key \"%s\"\n", iter->c_str());
            return false;
        }
        *modmask |= cur_mask;
    }
    return true;
}

bool string2key(const std::string& str, unsigned int* modmask, KeySym* keysym) {
    if (!string2modifiers(str, modmask)) {
        return false;
    }
    // last one is the key
    auto lastToken = splitKeySpec(str).back();
    *keysym = XStringToKeysym(lastToken.c_str());
    if (*keysym == NoSymbol) {
        HSWarning("unknown KeySym \"%s\"\n", lastToken.c_str());
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
    auto found = std::find_if(g_key_binds.begin(), g_key_binds.end(),
            [=](const unique_ptr<KeyBinding> &other) {
                return keysym_equals(&pressed, other.get()) == 0;
                });
    if (found != g_key_binds.end()) {
        // call the command
        std::ostringstream discardedOutput;
        auto& cmd = (*found)->cmd;
        Input input(cmd.front(), {cmd.begin() + 1, cmd.end()});
        Commands::call(input, discardedOutput);
    }
}

bool key_remove_bind_with_keysym(unsigned int modifiers, KeySym keysym){
    KeyBinding bind;
    bind.modifiers = modifiers;
    bind.keysym = keysym;
    // search this keysym in list and remove it
    for (auto iter = g_key_binds.begin(); iter != g_key_binds.end(); iter++) {
        if (keysym_equals(&bind, iter->get()) == 0) {
            g_key_binds.erase(iter);
            return true;
        }
    }
    return false;
}

void regrab_keys() {
    update_numlockmask();
    // init modifiers after updating numlockmask
    XUngrabKey(g_display, AnyKey, AnyModifier, g_root); // remove all current grabs
    for (auto& binding : g_key_binds) {
        grab_keybind(binding.get());
    }
}

void grab_keybind(KeyBinding* binding) {
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
 * Converts the given binding to a string
 */
std::string keybinding_to_string(KeyBinding* binding) {
    std::stringstream str;

    /* add modifiers */
    unsigned int old_mask = 0, new_mask = binding->modifiers;
    while (new_mask != 0 && new_mask != old_mask) {
        old_mask = new_mask;

        /* try to find a modifier */
        const char* name = modifiermask2name(old_mask);
        if (!name) {
            break;
        }
        str << name << KEY_COMBI_SEPARATORS[0];
        /* remove found mask from mask */
        new_mask = old_mask & ~ modifiername2mask(name);
    }

    /* add keysym */
    const char* name = XKeysymToString(binding->keysym);
    if (!name) {
        g_warning("XKeysymToString failed! using \'?\' instead\n");
        name = "?";
    }
    str << name;

    return str.str();
}

// STRTODO
struct key_find_context {
    Output      output;
    const char* needle;
    size_t      needle_len;
};

static void key_find_binds_helper(KeyBinding* b, struct key_find_context* c) {
    auto name = keybinding_to_string(b);
    if (name.find(c->needle) == 0) {
        /* add to output if key starts with searched needle */
        c->output << name << std::endl;
    }
}

void key_find_binds(const char* needle, Output output) {
    struct key_find_context c = {
        output, needle, strlen(needle)
    };
    for (auto& binding : g_key_binds) {
        key_find_binds_helper(binding.get(), &c);
    }
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
        auto name = keybinding_to_string(b);
        regmatch_t match;
        int status = regexec(keymask_regex, name.c_str(), 1, &match, 0);
        // only accept it, if it matches the entire string
        if (status == 0
            && match.rm_so == 0
            && match.rm_eo == name.length()) {
            enabled = true;
        } else {
            // Keybinding did not match, therefore we disable it
            enabled = false;
        }
    }

    if (enabled && !b->enabled) {
        grab_keybind(b);
    } else if(!enabled && b->enabled) {
        ungrab_keybind(b, nullptr);
    }
}

void key_set_keymask(HSTag *tag, Client *client) {
    regex_t     keymask_regex;
    if (client && client->keymask_ != "") {
        int status = regcomp(&keymask_regex, ((std::string)client->keymask_).c_str(), REG_EXTENDED);
        if (status == 0) {
            for (auto& binding : g_key_binds) {
                key_set_keymask_helper(binding.get(), &keymask_regex);
            }
            return;
        } else {
            char buf[ERROR_STRING_BUF_SIZE];
            regerror(status, &keymask_regex, buf, ERROR_STRING_BUF_SIZE);
            HSDebug("keymask: Can not parse regex \"%s\" from keymask: %s",
                    ((std::string)client->keymask_).c_str(), buf);
        }
    }
    // Enable all keys again
    for (auto& binding : g_key_binds) {
        key_set_keymask_helper(binding.get(), 0);
    }
}
