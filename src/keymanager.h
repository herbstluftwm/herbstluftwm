#pragma once

#include <X11/Xlib.h>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "keycombo.h"
#include "object.h"
#include "types.h"
#include "xkeygrabber.h"

class Client;
class Completion;

/*!
 * Maintains the list of key bindings, and handles the grabbing/ungrabbing with
 * the help of XKeyGrabber
 */
class KeyManager : public Object {
    /*!
     * Simple parser/container for a keymask regex (only needed internally by
     * KeyManager)
     */
    class KeyMask {
    public:
        /*!
         * Creates a Keymask object from the given regex string
         *
         * /throws exceptions thrown by std::regex
         */
        static KeyMask fromString(const std::string& str = "") {
            KeyMask ret;
            if (str != "") {
                // Simply pass on any exceptions thrown here:
                ret.regex = std::regex(str, std::regex::extended);
            }
            return ret;
        }

        bool operator==(const KeyMask& other) const {
            return other.str == str;
        }

        std::string str;
        std::regex regex;
    };

    /*!
     * Simple container class for tracking a keybinding (only supposed to be
     * needed internally by KeyManager)
     */
public: // still public for now because it is used elsewhere (TODO: fix that)
    class KeyBinding {
    public:
        KeyCombo keyCombo;
        std::vector<std::string> cmd;
        bool grabbed;
    };

public:
    KeyManager() = default;
    ~KeyManager();

    int addKeybindCommand(Input input, Output output);
    int listKeybindsCommand(Output output) const;
    int removeKeybindCommand(Input input, Output output);

    void addKeybindCompletion(Completion &complete);
    void removeKeybindCompletion(Completion &complete);

    void handleKeyPress(XEvent* ev) const;

    void regrabAll();
    void ensureKeyMask(const Client* client = nullptr);
    void setActiveKeyMask(const KeyMask& newMask);
    void clearActiveKeyMask();

    // TODO: This is not supposed to exist. It only does as a workaround,
    // because mouse.cpp still wants to know the numlock mask.
    unsigned int getNumlockMask() const {
        return xKeyGrabber_.getNumlockMask();
    }

    //! Currently defined keybindings (TODO: Make this private as soon as possible)
    std::vector<std::unique_ptr<KeyBinding>> binds;

private:
    bool removeKeyBinding(const KeyCombo& comboToRemove);

    XKeyGrabber xKeyGrabber_;

    // The last known keymask (for comparison on change)
    KeyMask activeKeyMask_;
};
