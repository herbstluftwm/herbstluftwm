#include "keymanager.h"

#include <algorithm>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>

#include "arglist.h"
#include "clientmanager.h"
#include "command.h"
#include "completion.h"
#include "globals.h"
#include "ipc-protocol.h"
#include "keycombo.h"
#include "root.h"
#include "utils.h"

using std::string;
using std::unique_ptr;

KeyManager::~KeyManager() {
    xKeyGrabber_.ungrabAll();
}

int KeyManager::addKeybindCommand(Input input, Output output) {
    if (input.size() < 2) {
        return HERBST_NEED_MORE_ARGS;
    }

    auto newBinding = make_unique<KeyBinding>();

    try {
        newBinding->keyCombo = KeyCombo::fromString(input.front());
    } catch (std::runtime_error &error) {
        output << input.command() << ": " << error.what() << std::endl;
        return HERBST_INVALID_ARGUMENT;
    }

    input.shift();
    // Store remaining input as the associated command
    newBinding->cmd = {input.begin(), input.end()};

    // Make sure there is no existing binding with same keysym/modifiers
    removeKeybinding(newBinding->keyCombo);

    if (!newBinding->keyCombo.matches(activeKeyMask_.regex)) {
        // Grab for events on this keycode
        xKeyGrabber_.grabKeyCombo(newBinding->keyCombo);
        newBinding->grabbed = true;
    }

    // Add keybinding to list
    binds.push_back(std::move(newBinding));

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
        KeyCombo comboToRemove;
        try {
            comboToRemove = KeyCombo::fromString(arg);
        } catch (std::runtime_error &error) {
            output << input.command() << ": " << arg << ": " << error.what() << "\n";
            return HERBST_INVALID_ARGUMENT;
        }

        // Remove binding (or moan if none was found)
        if (removeKeybinding(comboToRemove)) {
            regrabAll();
        } else {
            output << input.command() << ": Key \"" << arg << "\" is not bound\n";
        }
    }

    return HERBST_EXIT_SUCCESS;
}

void KeyManager::addKeybindCompletion(Completion &complete) {
    if (complete == 0) {
        auto needle = complete.needle();

        // Use the first separator char that appears in the needle as default:
        const string seps = KeyCombo::separators;
        string sep = {seps.front()};
        for (auto& needleChar : needle) {
            if (seps.find(needleChar) != std::string::npos) {
                sep = needleChar;
                break;
            }
        }

        // Normalize needle by chopping off tokens until they all are valid
        // modifiers:
        auto tokens = KeyCombo::tokensFromString(needle);
        while (tokens.size() > 0) {
            try {
                KeyCombo::modifierMaskFromTokens(tokens);
                break;
            } catch (std::runtime_error &error) {
                tokens.pop_back();
            }
        }

        auto normNeedle = join_strings(tokens, sep);
        normNeedle += tokens.empty() ? "" : sep;
        auto modifiersInNeedle = std::set<string>(tokens.begin(), tokens.end());

        // Offer partial completions for an additional modifier (excluding the
        // ones already mentioned in the needle):
        for (auto& modifier : KeyCombo::modifierMasks) {
            if (modifiersInNeedle.count(modifier.name) == 0) {
                complete.partial(normNeedle + modifier.name + sep);
            }
        }

        // Offer full completions for a final keysym:
        auto keySyms = XKeyGrabber::getPossibleKeySyms();
        for (auto keySym : keySyms) {
            complete.full(normNeedle + keySym);
        }
    } else {
        complete.completeCommands(1);
    }
}

void KeyManager::removeKeybindCompletion(Completion &complete) {
    if (complete == 0) {
        complete.full({ "-F", "--all" });

        for (auto& binding : binds) {
            complete.full(binding->keyCombo.str());
        }
    }
}

void KeyManager::handleKeyPress(XEvent* ev) const {
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
        Commands::call(input, discardedOutput);
    }
}

void KeyManager::regrabAll() {
    xKeyGrabber_.updateNumlockMask();

     // Remove all current grabs:
    xKeyGrabber_.ungrabAll();

    for (auto& binding : binds) {
        xKeyGrabber_.grabKeyCombo(binding->keyCombo);
        binding->grabbed = true;
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

    string targetMaskStr = (client != nullptr) ? client->keyMask_() : "";

    if (activeKeyMask_.str == targetMaskStr) {
        // nothing to do
        return;
    }

    try {
        auto newMask = KeyMask::fromString(targetMaskStr);
        setActiveKeyMask(newMask);
    } catch (std::regex_error& err) {
        HSWarning("Failed to parse keymask \"%s\"is invalid (falling back to empty mask): %s\n",
                targetMaskStr.c_str(), err.what());

        // Fall back to empty mask:
        setActiveKeyMask({});
    }
}

//! Apply new keymask by grabbing/ungrabbing current bindings accordingly
void KeyManager::setActiveKeyMask(const KeyMask& newMask) {
    for (auto& binding : binds) {
        auto name = binding->keyCombo.str();
        bool isMasked = binding->keyCombo.matches(newMask.regex);

        if (!isMasked && !binding->grabbed) {
            xKeyGrabber_.grabKeyCombo(binding->keyCombo);
            binding->grabbed = true;
        } else if (isMasked && binding->grabbed) {
            xKeyGrabber_.ungrabKeyCombo(binding->keyCombo);
            binding->grabbed = false;
        }
    }
    activeKeyMask_ = newMask;
}

//! Set the active keymask to an empty exception
void KeyManager::clearActiveKeyMask() {
    auto newMask = KeyMask::fromString("");
    setActiveKeyMask(newMask);
}

/*!
 * Removes a given key combo from the list of bindings (no ungrabbing)
 *
 * \return True if a matching binding was found and removed
 * \return False if no matching binding was found
 */
bool KeyManager::removeKeybinding(const KeyCombo& comboToRemove) {
    // Find binding to remove
    auto removeIter = binds.begin();
    for (; removeIter != binds.end(); removeIter++) {
        if (comboToRemove == (*removeIter)->keyCombo) {
            break;
        }
    }

    if (removeIter == binds.end()) {
        return False; // no matching binding found
    }

    // Remove binding
    binds.erase(removeIter);
    return True;
}
