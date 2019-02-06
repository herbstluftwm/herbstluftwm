#pragma once

#include <X11/Xlib.h>

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

    KeyCombo xEventToKeyCombo(XEvent *ev) const;

    void grabKeyCombo(const KeyCombo& keyCombo);
    void ungrabKeyCombo(const KeyCombo& keyCombo);
    void ungrabAll();

    // TODO: This is not supposed to exist. It only does as a workaround,
    // because mouse.cpp still wants to know the numlock mask.
    unsigned int getNumlockMask() const {
        return numlockMask_;
    }

private:
    void changeGrabbedState(const KeyCombo& keyCombo, bool grabbed);
    unsigned int numlockMask_ = 0;

};

