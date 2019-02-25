#include "mousemanager.h"

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <initializer_list>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "client.h"
#include "completion.h"
#include "globals.h"
#include "ipc-protocol.h"
#include "keycombo.h"
#include "mouse.h"
#include "utils.h"

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

void MouseManager::addMouseBindCompletion(Completion &complete) {
    if (complete == 0) {
        auto needle = complete.needle();

        // Use the first separator char that appears in the needle as default:
        const string seps = KeyCombo::separators;
        string sep = {seps.front()};
        for (auto& needleChar : needle) {
            if (seps.find(needleChar) != string::npos) {
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

        // Offer full completions for a mouse button:
        auto buttons = {
            "Button1",
            "Button2",
            "Button3",
            "Button4",
            "Button5",
            "B1",
            "B2",
            "B3",
            "B4",
            "B5",
        };
        for (auto button : buttons) {
            complete.full(normNeedle + button);
        }
    } else if (complete == 1) {
        complete.full({"move", "resize", "zoom", "call"});
    } else if (complete[1] == "call") {
        complete.completeCommands(2);
    } else {
        complete.none();
    }
}
