#include "mousemanager.h"

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <ostream>
#include <string>
#include <vector>

#include "client.h"
#include "clientmanager.h"
#include "command.h"
#include "completion.h"
#include "frametree.h"
#include "globals.h"
#include "ipc-protocol.h"
#include "keymanager.h"
#include "monitormanager.h"
#include "mouse.h"
#include "mousedraghandler.h"
#include "root.h"
#include "tag.h"
#include "utils.h"

using std::shared_ptr;
using std::vector;
using std::string;
using std::endl;

MouseManager::MouseManager()
    : dragHandler_({})
{
    /* set cursor theme */
    cursor = XCreateFontCursor(g_display, XC_left_ptr);
    XDefineCursor(g_display, g_root, cursor);

    mouseFunctions_ = {
        { "move",       &MouseManager::mouse_initiate_move },
        { "zoom",       &MouseManager::mouse_initiate_zoom },
        { "resize",     &MouseManager::mouse_initiate_resize },
        { "call",       &MouseManager::mouse_call_command },
    };
}

MouseManager::~MouseManager() {
    XFreeCursor(g_display, cursor);
}

void MouseManager::injectDependencies(ClientManager* clients, MonitorManager* monitors)
{
    clients_ = clients;
    monitors_ = monitors;
}

int MouseManager::addMouseBindCommand(Input input, Output output) {
    if (input.size() < 2) {
        return HERBST_NEED_MORE_ARGS;
    }

    auto mouseComboStr = input.front();

    MouseCombo mouseCombo;
    try {
        mouseCombo = Converter<MouseCombo>::parse(mouseComboStr);
    } catch (std::exception &error) {
        output << input.command() << ": " << error.what() << endl;
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

    if (action == &MouseManager::mouse_call_command && cmd.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }

    // Actually create the mouse binding
    MouseBinding mb;
    mb.mousecombo = mouseCombo;
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
        Converter<MouseCombo>::complete(complete, nullptr);
    } else if (complete == 1) {
        for (auto& it : mouseFunctions_) {
            complete.full(it.first);
        }
    } else if (complete[1] == "call") {
        complete.completeCommands(2);
    } else {
        complete.none();
    }
}

void MouseManager::mouse_handle_event(unsigned int modifiers, unsigned int button, Window window) {
    auto b = mouse_binding_find(modifiers, button);

    if (!b.has_value()) {
        // No binding find for this event
    }

    Client* client = get_client_from_window(window);
    if (!client) {
        // there is no valid bind for this type of mouse event
        return;
    }
    (this ->* (b->action))(client, b->cmd);
}

int MouseManager::dragCommand(Input input, Output output)
{
    string winid, mouseAction;
    if (!(input >> winid >> mouseAction)) {
        return HERBST_NEED_MORE_ARGS;
    }
    Client* client = Root::get()->clients->client(winid);
    if (!client) {
        output << "Could not find client \"" << winid << "\"\n";
        return HERBST_INVALID_ARGUMENT;
    }
    MouseFunction action = string2mousefunction(mouseAction.c_str());
    if (!action) {
        output << input.command() << ": Unknown mouse action \"" << mouseAction << "\"" << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    if (monitors_->byTag(client->tag()) == nullptr) {
        output << input.command() << ": can not drag invisible client \"" << winid << "\"" << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    (this ->* action)(client, input.toVector());
    return 0;
}

void MouseManager::dragCompletion(Completion& complete)
{
    if (complete == 0) {
        Root::get()->clients->completeClients(complete);
    } else if (complete == 1) {
        for (auto& it : mouseFunctions_) {
            complete.full(it.first);
        }
    } else if (complete[1] == "call") {
        complete.completeCommands(2);
    } else {
        complete.none();
    }
}

void MouseManager::mouse_initiate_move(Client* client, const vector<string> &cmd) {
    mouse_initiate_drag(
                client,
                MouseDragHandlerFloating::construct(
                    &MouseDragHandlerFloating::mouse_function_move));
}

void MouseManager::mouse_initiate_zoom(Client* client, const vector<string> &cmd) {
    MouseDragHandler::Constructor constructor;
    if (client->is_client_floated()) {
        constructor = MouseDragHandlerFloating::construct(
                         &MouseDragHandlerFloating::mouse_function_zoom);
    } else {
        auto frame = client->tag()->frame->findFrameWithClient(client);
        constructor = MouseResizeFrame::construct(frame);
    }
    mouse_initiate_drag(client, constructor);
}

void MouseManager::mouse_initiate_resize(Client* client, const vector<string> &cmd) {
    MouseDragHandler::Constructor constructor;
    if (client->is_client_floated()) {
        constructor = MouseDragHandlerFloating::construct(
                         &MouseDragHandlerFloating::mouse_function_resize);
    } else {
        auto frame = client->tag()->frame->findFrameWithClient(client);
        constructor = MouseResizeFrame::construct(frame);
    }
    mouse_initiate_drag(client, constructor);
}

void MouseManager::mouse_call_command(Client* client, const vector<string> &cmd) {
    // TODO: add completion
    clients_->setDragged(client);

    if (cmd.empty()) {
        return;
    }
    // Execute the bound command
    std::ostringstream discardedOutput;
    Input input(cmd.front(), {cmd.begin() + 1, cmd.end()});
    Commands::call(input, discardedOutput);

    clients_->setDragged(nullptr);
}

void MouseManager::mouse_initiate_drag(Client *client, const MouseDragHandler::Constructor& createHandler)
{
    try {
        dragHandler_ = createHandler(monitors_, client);
        // only grab pointer if dragHandler_ could be started
        clients_->setDragged(client);
        XGrabPointer(g_display, client->x11Window(), True,
            PointerMotionMask|ButtonReleaseMask, GrabModeAsync,
                GrabModeAsync, None, None, CurrentTime);
    }  catch (const MouseDragHandler::DragNotPossible&) {
        // clear all fields, just to be sure
        dragHandler_ = {};
        clients_->setDragged(nullptr);
    }
}

void MouseManager::mouse_stop_drag() {
    // end those operations that have been started by mouse_initiate_drag()
    if (dragHandler_) {
        clients_->setDragged(nullptr);
        try {
            dragHandler_->finalize();
        }  catch (const MouseDragHandler::DragNotPossible&) {
            // we can't do anything else if finalizing fails
        }
        dragHandler_ = {};
    }
    XUngrabPointer(g_display, CurrentTime);
    // remove all enternotify-events from the event queue that were
    // generated by the XUngrabPointer
    XEvent ev;
    XSync(g_display, False);
    while(XCheckMaskEvent(g_display, EnterWindowMask, &ev));
}

void MouseManager::handle_motion_event(Point2D newCursorPos) {
    if (!dragHandler_) {
        return;
    }
    try {
        dragHandler_->handle_motion_event(newCursorPos);
    }  catch (const MouseDragHandler::DragNotPossible&) {
        mouse_stop_drag();
    }
}

bool MouseManager::mouse_is_dragging() {
    return dragHandler_.get();
}

int MouseManager::mouse_unbind_all(Output) {
    binds.clear();
    Client* client = get_current_client();
    if (client) {
        grab_client_buttons(client, true);
    }
    return 0;
}

MouseFunction MouseManager::string2mousefunction(const string& name) {
    auto it = mouseFunctions_.find(name);
    if (it != mouseFunctions_.end()) {
        return it->second;
    } else {
        return nullptr;
    }
}

std::experimental::optional<MouseBinding> MouseManager::mouse_binding_find(unsigned int modifiers, unsigned int button) {
    unsigned int numlockMask = Root::get()->keys()->getNumlockMask();
    MouseCombo mb = {};
    mb.modifiers_ = modifiers
        & ~(numlockMask|LockMask) // remove numlock and capslock mask
        & ~( Button1Mask // remove all mouse button masks
           | Button2Mask
           | Button3Mask
           | Button4Mask
           | Button5Mask );
    mb.button_ = button;

    auto found = std::find_if(binds.begin(), binds.end(),
            [=](const MouseBinding &other) {
                return other.mousecombo == mb;
            });
    if (found != binds.end()) {
        return *found;
    } else {
        return {};
    }
}

static void grab_binding(MouseBinding* bind, Client* client) {
    unsigned int numlockMask = Root::get()->keys()->getNumlockMask();
    unsigned int modifiers[] = { 0, LockMask, numlockMask, numlockMask | LockMask };
    for(int j = 0; j < LENGTH(modifiers); j++) {
        XGrabButton(g_display, bind->mousecombo.button_,
                    bind->mousecombo.modifiers_ | modifiers[j],
                    client->x11Window(), False, ButtonPressMask | ButtonReleaseMask,
                    GrabModeAsync, GrabModeSync, None, None);
    }
}

void MouseManager::grab_client_buttons(Client* client, bool focused) {
    XUngrabButton(g_display, AnyButton, AnyModifier, client->x11Window());
    if (focused) {
        for (auto& bind : binds) {
            grab_binding(&bind, client);
        }
    }
    unsigned int btns[] = { Button1, Button2, Button3 };
    for (int i = 0; i < LENGTH(btns); i++) {
        XGrabButton(g_display, btns[i], AnyModifier, client->x11Window(), False,
                    ButtonPressMask|ButtonReleaseMask, GrabModeSync,
                    GrabModeSync, None, None);
    }
}
