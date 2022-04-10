#include "keymanager.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

#include "argparse.h"
#include "arglist.h"
#include "client.h"
#include "clientmanager.h"
#include "command.h"
#include "completion.h"
#include "ipc-protocol.h"
#include "keycombo.h"
#include "root.h"
#include "utils.h"

using std::endl;
using std::string;
using std::unique_ptr;

KeyManager::~KeyManager() {
    xKeyGrabber_.ungrabAll();
}

void KeyManager::keybindCommand(CallOrComplete invoc)
{
    KeyCombo key;
    ArgParse ap;
    ap.mandatory(key);
    ap.command(invoc,
               [&](Completion& complete) { complete.completeCommands(0); },
               [&](ArgList command, Output output) -> int {
        if (command.empty()) {
            return  HERBST_NEED_MORE_ARGS;
        }
        KeyBinding keybind;
        keybind.keyCombo = key;
        keybind.cmd = { command.begin(), command.end() };
        return addKeybind(keybind, output);
    });
}

int KeyManager::addKeybind(KeyBinding newBinding, Output output) {
    // newBinding->cmd is not empty because the size before the input.shift() was >= 2
    if (!Commands::commandExists(newBinding.cmd[0])) {
        output.perror() << "the command \""
               << newBinding.cmd[0] << "\" does not exist."
               << " Did you forget \"spawn\"?\n";
        return HERBST_COMMAND_NOT_FOUND;
    }

    // Make sure there is no existing binding with same keysym/modifiers
    bool alreadyActive = false;
    removeKeyBinding(newBinding.keyCombo, &alreadyActive);

    if (currentKeyMask_.allowsBinding(newBinding.keyCombo)
        && currentKeysInactive_.allowsBinding(newBinding.keyCombo))
    {
        // Grab for events on this keycode
        newBinding.grabbed = true;
        if (!alreadyActive) {
            xKeyGrabber_.grabKeyCombo(newBinding.keyCombo);
        }
    }

    // Add keybinding to list
    auto ptr = make_unique<KeyBinding>(newBinding);
    binds.push_back(std::move(ptr));

    ensureKeyMask();

    return HERBST_EXIT_SUCCESS;
}

int KeyManager::listKeybindsCommand(Output output) const {
    for (auto& binding : binds) {
        // add key combo
        output << binding->keyCombo.str();
        // add associated command
        output << "\t" << ArgList(binding->cmd).join('\t');
        output << "\n";
    }
    return 0;
}

int KeyManager::removeKeybindCommand(Input input, Output output) {
    string arg;
    if (!(input >> arg)) {
        return HERBST_NEED_MORE_ARGS;
    }

    if (arg == "--all" || arg == "-F") {
        binds.clear();
        xKeyGrabber_.ungrabAll();
    } else {
        KeyCombo comboToRemove = {};
        try {
            comboToRemove = Converter<KeyCombo>::parse(arg);
        } catch (std::exception &error) {
            output.perror() << arg << ": " << error.what() << "\n";
            return HERBST_INVALID_ARGUMENT;
        }

        // Remove binding (or moan if none was found)
        if (removeKeyBinding(comboToRemove)) {
            regrabAll();
        } else {
            output.perror() << "Key \"" << arg << "\" is not bound\n";
            return HERBST_INVALID_ARGUMENT;
        }
    }

    return HERBST_EXIT_SUCCESS;
}

void KeyManager::removeKeybindCompletion(Completion &complete) {
    if (complete == 0) {
        complete.full({ "-F", "--all" });

        for (auto& binding : binds) {
            complete.full(binding->keyCombo.str());
        }
    }
}

void KeyManager::handleKeyPress(XKeyEvent* ev) {
    KeyCombo pressed = xKeyGrabber_.xEventToKeyCombo(ev);

    auto found = std::find_if(binds.begin(), binds.end(),
            [=](const unique_ptr<KeyBinding> &other) {
                return pressed == other->keyCombo;
            });
    if (found != binds.end()) {
        // execute the bound command
        std::ostringstream discardedOutput;
        auto& cmd = (*found)->cmd;
        Input input(cmd.front(), {cmd.begin() + 1, cmd.end()});
        // discard output, but forward errors to std::cerr
        OutputChannels channels(cmd.front(), discardedOutput, std::cerr);
        Commands::call(input, channels);
    }
}

void KeyManager::regrabAll() {
    xKeyGrabber_.updateNumlockMask();

     // Remove all current grabs:
    xKeyGrabber_.ungrabAll();

    for (auto& binding : binds) {
        // grab precisely those again, that have been grabbed before
        if (binding->grabbed) {
            xKeyGrabber_.grabKeyCombo(binding->keyCombo);
        }
    }
}

/*!
 * Makes sure that the currently active keymask is correct for the currently
 * focused client and regrabs keys if necessary
 *
 * FIXME: The Client* argument only exists because I failed to find a place to
 * call this function on focus changes where ClientManager::focus is already
 * updated.
 */
void KeyManager::ensureKeyMask(const Client* client) {
    if (client == nullptr) {
        client = Root::get()->clients()->focus();
    }
    // if there is still no client, then nothing is focused
    if (client == nullptr) {
        clearActiveKeyMask();
        return;
    }

    // keyMask => keybinding is allowed if it matches
    //            the reg. expression => no negation
    KeyMask newKeyMask(client->keyMask_(), false);
    // keysInactive => keybinding is disallowed if it
    //                 *does not* match the reg. expression => negation!
    KeyMask newKeysInactive(client->keysInactive_(), true);

    if (currentKeyMask_ == newKeyMask
        && currentKeysInactive_ == newKeysInactive)
    {
        // nothing to do
        return;
    }
    setActiveKeyMask(newKeyMask, newKeysInactive);
}

//! Apply new keymask by grabbing/ungrabbing current bindings accordingly
void KeyManager::setActiveKeyMask(const KeyMask& keyMask, const KeyMask& keysInactive) {
    for (auto& binding : binds) {
        bool isAllowed = keysInactive.allowsBinding(binding->keyCombo)
                         && keyMask.allowsBinding(binding->keyCombo);
        if (isAllowed && !binding->grabbed) {
            xKeyGrabber_.grabKeyCombo(binding->keyCombo);
            binding->grabbed = true;
        } else if (!isAllowed && binding->grabbed) {
            xKeyGrabber_.ungrabKeyCombo(binding->keyCombo);
            binding->grabbed = false;
        }
    }
    currentKeyMask_ = keyMask;
    currentKeysInactive_ = keysInactive;
}

//! Set the current key filters to an empty exception
void KeyManager::clearActiveKeyMask() {
    setActiveKeyMask({}, {});
}

/*!
 * Removes a given key combo from the list of bindings (no ungrabbing)
 * \param wasActive is set to the value whether the removed keybinding was active.
 * \return True if a matching binding was found and was removed
 * \return False if no matching binding was found
 */
bool KeyManager::removeKeyBinding(const KeyCombo& comboToRemove, bool* wasActive) {
    // Find binding to remove
    auto removeIter = binds.begin();
    for (; removeIter != binds.end(); removeIter++) {
        if (comboToRemove == (*removeIter)->keyCombo) {
            break;
        }
    }

    if (removeIter == binds.end()) {
        if (wasActive) {
            *wasActive = false;
        }
        return False; // no matching binding found
    }

    // Remove binding
    if (wasActive) {
        *wasActive = (*removeIter)->grabbed;
    }
    binds.erase(removeIter);
    return True;
}


/*!
 * Returns true if the string representation of the KeyCombo matches
 * the given keymask
 */
KeyManager::KeyMask::KeyMask(const RegexStr &regex, bool negated)
    : regex_(regex)
    , negated_(negated)
{
}

KeyManager::KeyMask::KeyMask()
    : regex_(RegexStr::fromStr(""))
    , negated_(false)
{
}

bool KeyManager::KeyMask::allowsBinding(const KeyCombo &combo) const
{
    if (regex_.empty()) {
        // an unset keymask allows every binding, regardless of
        // the 'negated_' flag
        return true;
    } else {
        KeyCombo withoutRelease = combo;
        withoutRelease.onRelease_ = false;
        bool match = regex_.matches(withoutRelease.str());
        if (negated_) {
            // only allow keybindings that don't match
            return !match;
        } else {
            // only allow keybindings that match
            return match;
        }
    }
}
