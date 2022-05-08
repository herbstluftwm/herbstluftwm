#include "xkeygrabber.h"

#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "root.h"
#include "xconnection.h"

using std::vector;
using std::string;

XKeyGrabber::XKeyGrabber(XConnection& X)
    : X_(X)
{
    updateNumlockMask();
}

//! Obtains the current numlock mask value
void XKeyGrabber::updateNumlockMask() {
    XModifierKeymap *modmap;

    numlockMask_ = 0;
    modmap = XGetModifierMapping(X_.display());
    for (size_t i = 0; i < 8; i++) {
        for (int j = 0; j < modmap->max_keypermod; j++) {
            if (modmap->modifiermap[i * modmap->max_keypermod + j]
                    == XKeysymToKeycode(X_.display(), XK_Num_Lock)) {
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
 *
 * Also, for release-events, we look up the modifier state that was active during
 * the key press.
 */
KeyCombo XKeyGrabber::xEventToKeyCombo(XKeyEvent* ev) {
    KeyCombo combo = {};
    combo.keysym = XkbKeycodeToKeysym(X_.display(), ev->keycode, 0, 0);
    combo.modifiers_ =  ev->state & ~(numlockMask_ | LockMask);
    if (ev->type == KeyRelease) {
        // on key release: extract the modifier state from when
        // the corresponding key press happened
        auto it = keycode2modifierMask_.find(ev->keycode);
        if (it != keycode2modifierMask_.end()) {
            combo.modifiers_ = it->second;
            combo.modifiers_ &= ~(numlockMask_ | LockMask);
            // since we see the release event,
            // the modifier mask is not needed anymore:
            keycode2modifierMask_.erase(it);
        }
        combo.onRelease_ = true;
    } else {
        // on key press:
        // - keep the modifiers as extracted from ev->state.
        // - but remember them for the corresponding release event
        keycode2modifierMask_[ev->keycode] = combo.modifiers_;
    }

    return combo;
}

//! Grabs the given key combo
void XKeyGrabber::grabKeyCombo(KeyCombo keyCombo) {
    auto x11KeyCombo = keyCombo;
    x11KeyCombo.onRelease_ = false;
    int oldCount = keyComboCount(x11KeyCombo);
    setKeyComboCount(x11KeyCombo, oldCount + 1);
    if (oldCount <= 0) {
        changeGrabbedState(x11KeyCombo, true);
    }
}

//! Ungrabs the given key combo
void XKeyGrabber::ungrabKeyCombo(KeyCombo keyCombo) {
    auto x11KeyCombo = keyCombo;
    x11KeyCombo.onRelease_ = false;
    int oldCount = keyComboCount(x11KeyCombo);
    setKeyComboCount(x11KeyCombo, oldCount - 1);
    if (oldCount > 0 && oldCount - 1 <= 0) {
        changeGrabbedState(x11KeyCombo, false);
    }
}

//! Removes all grabbed keys (without knowing them)
void XKeyGrabber::ungrabAll() {
    XUngrabKey(X_.display(), AnyKey, AnyModifier, X_.root());
    keycombo2bindCount_.clear();
}

void XKeyGrabber::regrabAll()
{
    updateNumlockMask();

     // Remove all current grabs:
    XUngrabKey(X_.display(), AnyKey, AnyModifier, X_.root());

    // grab precisely those again, that have been grabbed before.
    // here, it is important that we grab each KeyCombo only once.
    // this works, because even if both press&release are grabbed, the
    // map has only one combined entry for both.
    for (auto& binding : keycombo2bindCount_) {
        changeGrabbedState(binding.first, true);
    }
}

//! Grabs/ungrabs a given key combo
void XKeyGrabber::changeGrabbedState(const KeyCombo& keyCombo, bool grabbed) {
    // List of ignored modifiers (key combo will be grabbed for each of them):
    const unsigned int ignModifiers[] = { 0, LockMask, numlockMask_, numlockMask_ | LockMask };

    KeyCode keycode = XKeysymToKeycode(X_.display(), keyCombo.keysym);
    if (!keycode) {
        // Ignore unknown keysym
        return;
    }

    // Grab/ungrab key for each modifier that is ignored (capslock, numlock)
    for (auto& ignModifier : ignModifiers) {
        if (grabbed) {
            XGrabKey(X_.display(), keycode, ignModifier | keyCombo.modifiers_, X_.root(),
                    True, GrabModeAsync, GrabModeAsync);
        } else {
            XUngrabKey(X_.display(), keycode, ignModifier | keyCombo.modifiers_, X_.root());
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
    if (newCount <= 0) {
        keycombo2bindCount_.erase(x11KeyCombo);
    } else {
        keycombo2bindCount_[x11KeyCombo] = newCount;
    }
}


vector<string> XKeyGrabber::getPossibleKeySyms() {
    vector<string> ret;
    int min, max;
    XConnection& X = Root::get()->X;
    XDisplayKeycodes(X.display(), &min, &max);
    int kc_count = max - min + 1;
    int ks_per_kc; // count of keysysms per keycode
    KeySym* keysyms;
    keysyms = XGetKeyboardMapping(X.display(), min, kc_count, &ks_per_kc);
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
