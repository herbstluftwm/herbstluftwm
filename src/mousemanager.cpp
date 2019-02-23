#include "mousemanager.h"

#include <X11/cursorfont.h>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "client.h"
#include "globals.h"
#include "ipc-protocol.h"
#include "keycombo.h"
#include "mouse.h"

using std::vector;
using std::string;
using std::endl;

MouseManager::MouseManager() {
    /* set cursor theme */
    cursor = XCreateFontCursor(g_display, XC_left_ptr);
    XDefineCursor(g_display, g_root, cursor);
}

MouseManager::~MouseManager() {
    XFreeCursor(g_display, cursor);
}

int MouseManager::addMouseBindCommand(Input input, Output output) {
    if (input.size() < 2) {
        return HERBST_NEED_MORE_ARGS;
    }

    auto mouseComboStr = input.front();

    auto tokens = KeyCombo::tokensFromString(mouseComboStr);
    unsigned int modifiers = 0;
    try {
        auto modifierSlice = vector<string>({tokens.begin(), tokens.end() - 1});
        modifiers = KeyCombo::modifierMaskFromTokens(modifierSlice);
    } catch (std::runtime_error &error) {
        output << input.command() << ": " << error.what() << endl;
        return HERBST_INVALID_ARGUMENT;
    }

    // Last token is the mouse button
    auto buttonStr = tokens.back();
    unsigned int button = string2button(buttonStr.c_str());
    if (button == 0) {
        output << input.command() << ": Unknown mouse button \"" << buttonStr << "\"" << endl;
        return HERBST_INVALID_ARGUMENT;
    }

    input.shift();
    auto action = string2mousefunction(input.front().c_str());
    if (!action) {
        output << input.command() << ": Unknown mouse action \"" << input.front() << "\"" << endl;
        return HERBST_INVALID_ARGUMENT;
    }

    input.shift();
    // Use remaining input as the associated command
    vector<string> cmd = {input.begin(), input.end()};

    if (action == mouse_call_command && cmd.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }

    // Actually create the mouse binding
    MouseBinding mb;
    mb.button = button;
    mb.modifiers = modifiers;
    mb.action = action;
    mb.cmd = cmd;
    binds.push_front(mb);
    Client* client = get_current_client();
    if (client) {
        grab_client_buttons(client, true);
    }

    return HERBST_EXIT_SUCCESS;
}
