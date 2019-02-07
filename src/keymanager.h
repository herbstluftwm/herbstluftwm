#pragma once

#include <X11/Xlib.h>
#include <memory>
#include <string>
#include <vector>

#include "keycombo.h"
#include "object.h"
#include "types.h"

// TODO: Turn this into a nested class of KeyManager (will be easier later on)
class KeyBinding {
public:
    KeyCombo keyCombo;

    //! Command to call
    std::vector<std::string> cmd;

    //! Whether this keybinding is currently grabbed
    bool enabled;
};

class KeyManager : public Object {
public:
    KeyManager();
    ~KeyManager();

    int addKeybindCommand(Input input, Output output);
    int listKeybindsCommand(Output output) const;
    int removeKeybindCommand(Input input, Output output);

    void removeKeybindCompletion(Completion &complete);

    void ensureKeymask();

    //! Currently defined keybindings (TODO: Make this private as soon as possible)
    std::vector<std::unique_ptr<KeyBinding>> binds;
};
