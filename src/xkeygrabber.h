#pragma once

#include <X11/Xlib.h>
#include <map>
#include <string>
#include <vector>

#include "keycombo.h"

/*!
 * Handles the actual grabbing/releasing of given key combinations, and
 * maintains knowledge of the current keyboard layout
 *
 * Expects to be notified about keyboard mapping changes so that it can keep
 * track of the current numlock mask value.
 */
class XKeyGrabber {
public:
    XKeyGrabber();

    void updateNumlockMask();

    KeyCombo xEventToKeyCombo(XKeyEvent *ev);

    void grabKeyCombo(const KeyCombo& keyCombo);
    void ungrabKeyCombo(const KeyCombo& keyCombo);
    void ungrabAll();

    // TODO: This is not supposed to exist. It only does as a workaround,
    // because mouse.cpp still wants to know the numlock mask.
    unsigned int getNumlockMask() const {
        return numlockMask_;
    }

    static std::vector<std::string> getPossibleKeySyms();

private:
    void changeGrabbedState(const KeyCombo& keyCombo, bool grabbed);
    unsigned int numlockMask_ = 0;
    // for each (X11-)keycombo, we count in how many keybinds it is used.
    // it might be used in multiple because there are binds for both key press
    // and key release:
    std::map<KeyCombo,int> keycombo2bindCount_;
    // for each key code, whenever we see a key down event, remember
    // the modifier mask, such that we can re-use it for the key up event.
    std::map<unsigned int, unsigned int> keycode2modifierMask_;
    int keyComboCount(const KeyCombo& x11KeyCombo);
    void setKeyComboCount(const KeyCombo& x11KeyCombo, int newCount);
};

