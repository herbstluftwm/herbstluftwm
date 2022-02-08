#include "xkeygrabber.h"

#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "globals.h"

using std::vector;
using std::string;

XKeyGrabber::XKeyGrabber() {
    updateNumlockMask();
}

//! Obtains the current numlock mask value
void XKeyGrabber::updateNumlockMask() {
    XModifierKeymap *modmap;

    numlockMask_ = 0;
    modmap = XGetModifierMapping(g_display);
    for (size_t i = 0; i < 8; i++) {
        for (int j = 0; j < modmap->max_keypermod; j++) {
            if (modmap->modifiermap[i * modmap->max_keypermod + j]
                    == XKeysymToKeycode(g_display, XK_Num_Lock)) {
                numlockMask_ = (1 << i);
            }
        }
    }
    XFreeModifiermap(modmap);
}

/*!
 * Derives a "normalized" KeyCombo from a given event.
 *
 * Normalization means stripping any ignored modifiers from the modifier mask
 * (including the runtime-defined Numlock mask).
 */
KeyCombo XKeyGrabber::xEventToKeyCombo(XKeyEvent* ev) const {
    KeyCombo combo = {};
    combo.keysym = XkbKeycodeToKeysym(g_display, ev->keycode, 0, 0);
    combo.modifiers_ = ev->state;

    // Normalize
    if (ev->type == KeyRelease) {
	combo.modifiers_ |= ReleaseMask;
    }
    combo.modifiers_ &= ~(numlockMask_ | LockMask);

    return combo;
}

//! Grabs the given key combo
void XKeyGrabber::grabKeyCombo(const KeyCombo& keyCombo) {
    changeGrabbedState(keyCombo, true);
}

//! Ungrabs the given key combo
void XKeyGrabber::ungrabKeyCombo(const KeyCombo& keyCombo) {
    changeGrabbedState(keyCombo, false);
}

//! Removes all grabbed keys (without knowing them)
void XKeyGrabber::ungrabAll() {
    XUngrabKey(g_display, AnyKey, AnyModifier, g_root);
}

//! Grabs/ungrabs a given key combo
void XKeyGrabber::changeGrabbedState(const KeyCombo& keyCombo, bool grabbed) {
    // List of ignored modifiers (key combo will be grabbed for each of them):
    const unsigned int ignModifiers[] = { 0, LockMask, numlockMask_, numlockMask_ | LockMask };

    if (keyCombo.modifiers_ & ReleaseMask) return;

    KeyCode keycode = XKeysymToKeycode(g_display, keyCombo.keysym);
    if (!keycode) {
        // Ignore unknown keysym
        return;
    }

    // Grab/ungrab key for each modifier that is ignored (capslock, numlock)
    for (auto& ignModifier : ignModifiers) {
        if (grabbed) {
            XGrabKey(g_display, keycode, ignModifier | keyCombo.modifiers_, g_root,
                    True, GrabModeAsync, GrabModeAsync);
        } else {
            XUngrabKey(g_display, keycode, ignModifier | keyCombo.modifiers_, g_root);
        }
    }
}

vector<string> XKeyGrabber::getPossibleKeySyms() {
    vector<string> ret;
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
            ret.push_back(str);
        }
    }
    XFree(keysyms);

    return ret;
}
