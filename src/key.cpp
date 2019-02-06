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
#include "keycombo.h"
#include "root.h"
#include "utils.h"

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
    for (auto& strToMask : KeyCombo::modifierMasks) {
        g_string_printf(buf, "%s%c", strToMask.name.c_str(), seperator);
        try_complete_prefix_partial(needle, buf->str, prefix, output);
    }
    g_string_free(buf, true);
}
