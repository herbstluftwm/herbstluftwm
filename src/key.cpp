#include "key.h"

#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <regex>
#include <string>

#include "command.h"
#include "glib-backports.h"
#include "globals.h"
#include "root.h"
#include "utils.h"

using std::unique_ptr;
using std::vector;

static unsigned int numlockmask = 0;
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask))

unsigned int* get_numlockmask_ptr() {
    return &numlockmask;
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
    auto& binds = Root::get()->keys()->binds;
    auto found = std::find_if(binds.begin(), binds.end(),
            [=](const unique_ptr<KeyBinding> &other) {
                return keysym_equals(&pressed, other.get()) == 0;
                });
    if (found != binds.end()) {
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
    auto& binds = Root::get()->keys()->binds;
    for (auto iter = binds.begin(); iter != binds.end(); iter++) {
        if (keysym_equals(&bind, iter->get()) == 0) {
            binds.erase(iter);
            return true;
        }
    }
    return false;
}

void ungrab_all() {
    update_numlockmask();
    // init modifiers after updating numlockmask
    XUngrabKey(g_display, AnyKey, AnyModifier, g_root); // remove all current grabs
}

void regrab_keys() {
    ungrab_all();
    for (auto& binding : Root::get()->keys()->binds) {
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

static void key_set_keymask_helper(KeyBinding* b, const std::regex &maskRegex) {
    // add keybinding
    auto name = keybinding_to_string(b);
    bool isMasked = std::regex_match(name, maskRegex);

    if (!isMasked && !b->enabled) {
        HSDebug("Not masking binding: %s\n", name.c_str());
        grab_keybind(b);
    } else if(isMasked && b->enabled) {
        HSDebug("Masking binding: %s\n", name.c_str());
        ungrab_keybind(b, nullptr);
    }
}

void key_set_keymask(const std::string& keymask) {
    try {
        if (keymask != "") {
            auto maskRegex = std::regex(keymask, std::regex::extended);
            for (auto& binding : Root::get()->keys()->binds) {
                key_set_keymask_helper(binding.get(), maskRegex);
            }
            return;
        } else {
            HSDebug("Ignoring empty keymask\n");
        }
    } catch(std::regex_error& err) {
        HSDebug("keymask: Can not parse regex \"%s\" from keymask: %s\n",
                keymask.c_str(), err.what());
    }

    // Failure fallthrough: Make sure that all bindings end up enabled.
    for (auto& binding : Root::get()->keys()->binds) {
        grab_keybind(binding.get());
    }
}
