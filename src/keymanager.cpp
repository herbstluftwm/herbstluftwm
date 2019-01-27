#include "keymanager.h"

#include <memory>

#include "clientmanager.h"
#include "completion.h"
#include "globals.h"
#include "ipc-protocol.h"
#include "key.h"
#include "root.h"
#include "utils.h"

using std::vector;
using std::unique_ptr;

extern vector<unique_ptr<KeyBinding>> g_key_binds;

int KeyManager::addKeybindCommand(Input input, Output output) {
    if (input.size() < 2) {
        return HERBST_NEED_MORE_ARGS;
    }

    auto newBinding = make_unique<KeyBinding>();

    // Extract modifiers/keysym
    if (!string2key(input.front(), &(newBinding->modifiers), &(newBinding->keysym))) {
        output << input.command() << ": No such KeySym/modifier\n";
        return HERBST_INVALID_ARGUMENT;
    }

    input.shift();
    // Store remaining input as the associated command
    newBinding->cmd = {input.begin(), input.end()};

    // Remove existing binding with same keysym/modifiers
    key_remove_bind_with_keysym(newBinding->modifiers, newBinding->keysym);

    // Grab for events on this keycode
    grab_keybind(newBinding.get());

    // Add keybinding to list
    g_key_binds.push_back(std::move(newBinding));

    // Reapply the current keymask (if any)
    auto focusedClient = Root::get()->clients()->focus();
    if (focusedClient != nullptr) {
        key_set_keymask(focusedClient->keymask_());
    }

    return HERBST_EXIT_SUCCESS;
}

int KeyManager::listKeybindsCommand(Output output) {
    for (auto& binding : g_key_binds) {
        // add keybinding
        output << keybinding_to_string(binding.get());
        // add associated command
        output << "\t" << ArgList(binding->cmd).join('\t');
        output << "\n";
    }
    return 0;
}

int KeyManager::removeKeybindCommand(Input input, Output output) {
    std::string arg;
    if (!(input >> arg)) {
        return HERBST_NEED_MORE_ARGS;
    }

    if (arg == "--all" || arg == "-F") {
        key_remove_all_binds();
    } else {
        unsigned int modifiers;
        KeySym keysym;
        // get keycode
        if (!string2key(arg, &modifiers, &keysym)) {
            output << arg << ": No such KeySym/modifier\n";
            return HERBST_INVALID_ARGUMENT;
        }
        if (key_remove_bind_with_keysym(modifiers, keysym) == false) {
            output << input.command() << ": Key \"" << arg << "\" is not bound\n";
        }
        regrab_keys();
    }

    return HERBST_EXIT_SUCCESS;
}
