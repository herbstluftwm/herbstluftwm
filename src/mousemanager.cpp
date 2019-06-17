#include "mousemanager.h"

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <cstring>
#include <ostream>
#include <string>
#include <vector>

#include "client.h"
#include "clientmanager.h"
#include "command.h"
#include "completion.h"
#include "globals.h"
#include "ipc-protocol.h"
#include "keymanager.h"
#include "monitor.h"
#include "monitormanager.h"
#include "mouse.h"
#include "root.h"
#include "tag.h"
#include "utils.h"
#include "x11-utils.h"

using std::vector;
using std::string;
using std::endl;

MouseManager::MouseManager()
    : mode_(Mode::NoDrag)
{
    /* set cursor theme */
    cursor = XCreateFontCursor(g_display, XC_left_ptr);
    XDefineCursor(g_display, g_root, cursor);
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
        complete.full({"move", "resize", "zoom", "call"});
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
    (this ->* (b->action))(client, b->cmd); }

void MouseManager::mouse_initiate_move(Client* client, const vector<string> &cmd) {
    mouse_initiate_drag(client, &MouseManager::mouse_function_move);
}

void MouseManager::mouse_initiate_zoom(Client* client, const vector<string> &cmd) {
    mouse_initiate_drag(client, &MouseManager::mouse_function_zoom);
}

void MouseManager::mouse_initiate_resize(Client* client, const vector<string> &cmd) {
    mouse_initiate_drag(client, &MouseManager::mouse_function_resize);
}

void MouseManager::mouse_call_command(Client* client, const vector<string> &cmd) {
    // TODO: add completion
    clients_->setDragged(client);

    // Execute the bound command
    std::ostringstream discardedOutput;
    Input input(cmd.front(), {cmd.begin() + 1, cmd.end()});
    Commands::call(input, discardedOutput);

    clients_->setDragged(nullptr);
}


void MouseManager::mouse_initiate_drag(Client* client, MouseDragFunction function) {
    dragFunction_ = function;
    winDragClient_ = client;
    dragMonitor_ = monitors_->byTag(client->tag());
    if (!dragMonitor_ || client->tag()->floating == false) {
        // only can drag wins in  floating mode
        winDragClient_ = nullptr;
        dragFunction_ = nullptr;
        return;
    }
    mode_ = Mode::DraggingClient;
    dragMonitorIndex_ = dragMonitor_->index();
    clients_->setDragged(winDragClient_);
    winDragStart_ = winDragClient_->float_size_;
    buttonDragStart_ = get_cursor_position();
    XGrabPointer(g_display, client->x11Window(), True,
        PointerMotionMask|ButtonReleaseMask, GrabModeAsync,
            GrabModeAsync, None, None, CurrentTime);
}

bool MouseManager::draggingIsStillSafe() {
    if (monitors_->byIdx(dragMonitorIndex_) != dragMonitor_) {
        dragMonitor_ = nullptr;
    }
    return dragMonitor_
        && winDragClient_;
}

void MouseManager::mouse_stop_drag() {
    if (winDragClient_) {
        clients_->setDragged(nullptr);
        // resend last size
        if (dragMonitor_) {
            dragMonitor_->applyLayout();
        }
    }
    mode_ = Mode::NoDrag;
    winDragClient_ = nullptr;
    dragFunction_ = nullptr;
    XUngrabPointer(g_display, CurrentTime);
    // remove all enternotify-events from the event queue that were
    // generated by the XUngrabPointer
    XEvent ev;
    XSync(g_display, False);
    while(XCheckMaskEvent(g_display, EnterWindowMask, &ev));
}

void MouseManager::handle_motion_event(Point2D newCursorPos) {
    if (!draggingIsStillSafe()) {
        mouse_stop_drag();
        return;
    }
    if (mode_ == Mode::NoDrag) {
        return;
    }
    if (!dragFunction_) return;
    // call function that handles it
    (this ->* dragFunction_)(newCursorPos);
}

bool MouseManager::mouse_is_dragging() {
    return mode_ != Mode::NoDrag;
}

int MouseManager::mouse_unbind_all(Output) {
    binds.clear();
    Client* client = get_current_client();
    if (client) {
        grab_client_buttons(client, true);
    }
    return 0;
}

MouseFunction MouseManager::string2mousefunction(const char* name) {
    static struct {
        const char* name;
        MouseFunction function;
    } table[] = {
        { "move",       &MouseManager::mouse_initiate_move },
        { "zoom",       &MouseManager::mouse_initiate_zoom },
        { "resize",     &MouseManager::mouse_initiate_resize },
        { "call",       &MouseManager::mouse_call_command },
    };
    int i;
    for (i = 0; i < LENGTH(table); i++) {
        if (!strcmp(table[i].name, name)) {
            return table[i].function;
        }
    }
    return nullptr;
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

void MouseManager::mouse_function_move(Point2D newCursorPos) {
    int x_diff = newCursorPos.x - buttonDragStart_.x;
    int y_diff = newCursorPos.y - buttonDragStart_.y;
    winDragClient_->float_size_ = winDragStart_;
    winDragClient_->float_size_.x += x_diff;
    winDragClient_->float_size_.y += y_diff;
    // snap it to other windows
    int dx, dy;
    client_snap_vector(winDragClient_, dragMonitor_,
                       SNAP_EDGE_ALL, &dx, &dy);
    winDragClient_->float_size_.x += dx;
    winDragClient_->float_size_.y += dy;
    winDragClient_->resize_floating(dragMonitor_, get_current_client() == winDragClient_);
}

void MouseManager::mouse_function_resize(Point2D newCursorPos) {
    int x_diff = newCursorPos.x - buttonDragStart_.x;
    int y_diff = newCursorPos.y - buttonDragStart_.y;
    winDragClient_->float_size_ = winDragStart_;
    // relative x/y coords in drag window
    Monitor* m = dragMonitor_;
    int rel_x = m->relativeX(buttonDragStart_.x) - winDragStart_.x;
    int rel_y = m->relativeY(buttonDragStart_.y) - winDragStart_.y;
    bool top = false;
    bool left = false;
    if (rel_y < winDragStart_.height/2) {
        top = true;
        y_diff *= -1;
    }
    if (rel_x < winDragStart_.width/2) {
        left = true;
        x_diff *= -1;
    }
    // avoid an overflow
    int new_width  = winDragClient_->float_size_.width + x_diff;
    int new_height = winDragClient_->float_size_.height + y_diff;
    Client* client = winDragClient_;
    if (left)   winDragClient_->float_size_.x -= x_diff;
    if (top)    winDragClient_->float_size_.y -= y_diff;
    winDragClient_->float_size_.width  = new_width;
    winDragClient_->float_size_.height = new_height;
    // snap it to other windows
    int dx, dy;
    int snap_flags = 0;
    if (left)   snap_flags |= SNAP_EDGE_LEFT;
    else        snap_flags |= SNAP_EDGE_RIGHT;
    if (top)    snap_flags |= SNAP_EDGE_TOP;
    else        snap_flags |= SNAP_EDGE_BOTTOM;
    client_snap_vector(winDragClient_, dragMonitor_,
                       (SnapFlags)snap_flags, &dx, &dy);
    if (left) {
        winDragClient_->float_size_.x += dx;
        dx *= -1;
    }
    if (top) {
        winDragClient_->float_size_.y += dy;
        dy *= -1;
    }
    winDragClient_->float_size_.width += dx;
    winDragClient_->float_size_.height += dy;
    client->applysizehints(&winDragClient_->float_size_.width,
                           &winDragClient_->float_size_.height);
    if (left) {
        client->float_size_.x =
            winDragStart_.x + winDragStart_.width
            - winDragClient_->float_size_.width;
    }
    if (top) {
        client->float_size_.y =
            winDragStart_.y + winDragStart_.height
            - winDragClient_->float_size_.height;
    }
    winDragClient_->resize_floating(dragMonitor_, get_current_client() == winDragClient_);
}

void MouseManager::mouse_function_zoom(Point2D newCursorPos) {
    // stretch, where center stays at the same position
    int x_diff = newCursorPos.x - buttonDragStart_.x;
    int y_diff = newCursorPos.y - buttonDragStart_.y;
    // relative x/y coords in drag window
    Monitor* m = dragMonitor_;
    int rel_x = m->relativeX(buttonDragStart_.x) - winDragStart_.x;
    int rel_y = m->relativeY(buttonDragStart_.y) - winDragStart_.y;
    int cent_x = winDragStart_.x + winDragStart_.width  / 2;
    int cent_y = winDragStart_.y + winDragStart_.height / 2;
    if (rel_x < winDragStart_.width/2) {
        x_diff *= -1;
    }
    if (rel_y < winDragStart_.height/2) {
        y_diff *= -1;
    }
    Client* client = winDragClient_;

    // avoid an overflow
    int new_width  = winDragStart_.width  + 2 * x_diff;
    int new_height = winDragStart_.height + 2 * y_diff;
    // apply new rect
    client->float_size_.x = cent_x - new_width / 2;
    client->float_size_.y = cent_y - new_height / 2;
    client->float_size_.width = new_width;
    client->float_size_.height = new_height;
    // snap it to other windows
    int right_dx, bottom_dy;
    int left_dx, top_dy;
    // we have to distinguish the direction in which we zoom
    client_snap_vector(winDragClient_, m,
                     (SnapFlags)(SNAP_EDGE_BOTTOM | SNAP_EDGE_RIGHT), &right_dx, &bottom_dy);
    client_snap_vector(winDragClient_, m,
                       (SnapFlags)(SNAP_EDGE_TOP | SNAP_EDGE_LEFT), &left_dx, &top_dy);
    // e.g. if window snaps by vector (3,3) at topleft, window has to be shrinked
    // but if the window snaps by vector (3,3) at bottomright, window has to grow
    if (abs(right_dx) < abs(left_dx)) {
        right_dx = -left_dx;
    }
    if (abs(bottom_dy) < abs(top_dy)) {
        bottom_dy = -top_dy;
    }
    new_width += 2 * right_dx;
    new_height += 2 * bottom_dy;
    client->applysizehints(&new_width, &new_height);
    // center window again
    client->float_size_.width = new_width;
    client->float_size_.height = new_height;
    client->float_size_.x = cent_x - new_width / 2;
    client->float_size_.y = cent_y - new_height / 2;
    winDragClient_->resize_floating(dragMonitor_, get_current_client() == winDragClient_);
}
