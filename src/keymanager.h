#pragma once

#include <memory>
#include <string>
#include <vector>

#include "commandio.h"
#include "keycombo.h"
#include "object.h"
#include "regexstr.h"

class Client;
class Completion;

/*!
 * Maintains the list of key bindings, and handles the grabbing/ungrabbing with
 * the help of XKeyGrabber
 */
class KeyManager : public Object {
private:
    /*!
     * Simple parser/container for a keymask regex (only needed internally by
     * KeyManager)
     */
    class KeyMask {
    public:
        /*!
         * Keymask object essentially is a regex
         *
         * if negated=false, then this keymask only allows those keybindings
         * that match the given regex.
         *
         * if negated=true, then this keymask only allows those keybindings
         * that do not match the given regex.
         *
         * if the regex is "", then every keybinding is allowed (regardless of 'negated')
         */
        KeyMask(const RegexStr& regex, bool negated);
        KeyMask();

        bool allowsBinding(const KeyCombo& combo) const;
        std::string str() const { return regex_.str(); }

        bool operator==(const KeyMask& other) const {
            return  (other.regex_.empty() && regex_.empty())
                 || (other.regex_ == regex_ && other.negated_ == negated_);
        }
    private:
        RegexStr    regex_;
        bool        negated_ = true;
    };

    /*!
     * Simple container class for tracking a keybinding (only used internally
     * by KeyManager)
     */
    class KeyBinding {
    public:
        KeyCombo keyCombo;
        std::vector<std::string> cmd;
        bool grabbed = false;
    };

public:
    KeyManager() = default;
    ~KeyManager();

    //! emitted whenever a keycombo changes from inactive to active
    Signal_<KeyCombo> keyComboActive;
    //! emitted whenever a keycombo changes from active to inactive
    Signal_<KeyCombo> keyComboInactive;
    //! emitted when all keycombos become inactive
    Signal keyComboAllInactive;

    void keybindCommand(CallOrComplete invoc);
    int addKeybind(KeyBinding newBinding, Output output);
    int listKeybindsCommand(Output output) const;
    int removeKeybindCommand(Input input, Output output);

    void removeKeybindCompletion(Completion &complete);

    void handleKeyComboEvent(KeyCombo combo);

    void ensureKeyMask(const Client* client = nullptr);
    void setActiveKeyMask(const KeyMask& keyMask, const KeyMask& keysInactive);
    void clearActiveKeyMask();

private:
    bool removeKeyBinding(const KeyCombo& comboToRemove, bool* wasActive = nullptr);

    //! Currently defined keybindings
    std::vector<std::unique_ptr<KeyBinding>> binds;

    // The last applies KeyMask & KeysInactive(for comparison on change)
    KeyMask currentKeyMask_;
    KeyMask currentKeysInactive_;
};
