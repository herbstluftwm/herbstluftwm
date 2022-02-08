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
    combo.modifiers_ = ev->state | ((ev->type == KeyRelease) ? HlwmReleaseMask : 0);

    // Normalize
    combo.modifiers_ &= ~(numlockMask_ | LockMask);

    return combo;
}

//! Grabs the given key combo
void XKeyGrabber::grabKeyCombo(const KeyCombo& keyCombo) {
    auto x11KeyCombo = keyCombo.withoutEventModifiers();
    int oldCount = keyComboCount(x11KeyCombo);
    setKeyComboCount(x11KeyCombo, oldCount + 1);
    if (oldCount <= 0) {
        changeGrabbedState(x11KeyCombo, true);
    }
}

//! Ungrabs the given key combo
void XKeyGrabber::ungrabKeyCombo(const KeyCombo& keyCombo) {
    auto x11KeyCombo = keyCombo.withoutEventModifiers();
    int oldCount = keyComboCount(x11KeyCombo);
    setKeyComboCount(x11KeyCombo, oldCount - 1);
    if (oldCount > 0 && oldCount - 1 <= 0) {
        changeGrabbedState(x11KeyCombo, false);
    }
}

//! Removes all grabbed keys (without knowing them)
void XKeyGrabber::ungrabAll() {
    XUngrabKey(g_display, AnyKey, AnyModifier, g_root);
    keycombo2bindCount_.clear();
}

//! Grabs/ungrabs a given key combo
void XKeyGrabber::changeGrabbedState(const KeyCombo& keyCombo, bool grabbed) {
    // List of ignored modifiers (key combo will be grabbed for each of them):
    const unsigned int ignModifiers[] = { 0, LockMask, numlockMask_, numlockMask_ | LockMask };

    KeyCode keycode = XKeysymToKeycode(g_display, keyCombo.keysym);
    if (!keycode) {
        // Ignore unknown keysym
        return;
    }

    HSDebug("setting grabbed state of %s to %d\n", keyCombo.str().c_str(), grabbed);
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

int XKeyGrabber::keyComboCount(const KeyCombo& x11KeyCombo)
{
    auto it = keycombo2bindCount_.find(x11KeyCombo);
    if (it != keycombo2bindCount_.end()) {
        return it->second;
    }
    return 0;
}

void XKeyGrabber::setKeyComboCount(const KeyCombo& x11KeyCombo, int newCount)
{
    HSDebug("count[%s] = %d\n", x11KeyCombo.str().c_str(), newCount);
    if (newCount <= 0) {
        keycombo2bindCount_.erase(x11KeyCombo);
    } else {
        keycombo2bindCount_[x11KeyCombo] = newCount;
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
