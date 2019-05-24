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
#include "command.h"
#include "completion.h"
#include "globals.h"
#include "ipc-protocol.h"
#include "keycombo.h"
#include "keymanager.h"
#include "monitor.h"
#include "mouse.h"
#include "root.h"
#include "tag.h"
#include "utils.h"
#include "x11-utils.h"

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

    if (action == &MouseManager::mouse_call_command && cmd.empty()) {
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


static Point2D          buttonDragStart_;
static Rectangle        winDragStart_;
static Client*        winDragClient_ = nullptr;
static Monitor*       dragMonitor_ = nullptr;
static MouseDragFunction dragFunction_ = nullptr;

void MouseManager::mouse_handle_event(XEvent* ev) {
    XButtonEvent* be = &(ev->xbutton);
    auto b = mouse_binding_find(be->state, be->button);

    if (!b.has_value()) {
        // No binding find for this event
    }

    Client* client = get_client_from_window(ev->xbutton.window);
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
    client->set_dragged(true);

    // Execute the bound command
    std::ostringstream discardedOutput;
    Input input(cmd.front(), {cmd.begin() + 1, cmd.end()});
    Commands::call(input, discardedOutput);

    client->set_dragged(false);
}


void MouseManager::mouse_initiate_drag(Client* client, MouseDragFunction function) {
    dragFunction_ = function;
    winDragClient_ = client;
    dragMonitor_ = find_monitor_with_tag(client->tag());
    if (!dragMonitor_ || client->tag()->floating == false) {
        // only can drag wins in  floating mode
        winDragClient_ = nullptr;
        dragFunction_ = nullptr;
        return;
    }
    winDragClient_->set_dragged( true);
    winDragStart_ = winDragClient_->float_size_;
    buttonDragStart_ = get_cursor_position();
    XGrabPointer(g_display, client->x11Window(), True,
        PointerMotionMask|ButtonReleaseMask, GrabModeAsync,
            GrabModeAsync, None, None, CurrentTime);
}

void MouseManager::mouse_stop_drag() {
    if (winDragClient_) {
        winDragClient_->set_dragged(false);
        // resend last size
        dragMonitor_->applyLayout();
    }
    winDragClient_ = nullptr;
    dragFunction_ = nullptr;
    XUngrabPointer(g_display, CurrentTime);
    // remove all enternotify-events from the event queue that were
    // generated by the XUngrabPointer
    XEvent ev;
    XSync(g_display, False);
    while(XCheckMaskEvent(g_display, EnterWindowMask, &ev));
}

void MouseManager::handle_motion_event(XEvent* ev) {
    if (!dragMonitor_) { return; }
    if (!winDragClient_) return;
    if (!dragFunction_) return;
    if (ev->type != MotionNotify) return;
    // get newest motion notification
    while (XCheckMaskEvent(g_display, ButtonMotionMask, ev));
    // call function that handles it
    (this ->* dragFunction_)(&(ev->xmotion));
}

bool MouseManager::mouse_is_dragging() {
    return dragFunction_ != nullptr;
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

static struct {
    const char* name;
    unsigned int button;
} string2button_table[] = {
    { "Button1",       Button1 },
    { "Button2",       Button2 },
    { "Button3",       Button3 },
    { "Button4",       Button4 },
    { "Button5",       Button5 },
    { "B1",       Button1 },
    { "B2",       Button2 },
    { "B3",       Button3 },
    { "B4",       Button4 },
    { "B5",       Button5 },
};
unsigned int MouseManager::string2button(const char* name) {
    for (int i = 0; i < LENGTH(string2button_table); i++) {
        if (!strcmp(string2button_table[i].name, name)) {
            return string2button_table[i].button;
        }
    }
    return 0;
}


std::experimental::optional<MouseBinding> MouseManager::mouse_binding_find(unsigned int modifiers, unsigned int button) {
    MouseBinding mb = {};
    mb.modifiers = modifiers;
    mb.button = button;

    auto found = std::find_if(binds.begin(), binds.end(),
            [=](const MouseBinding &other) {
                return mouse_binding_equals(&other, &mb) == 0;
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
        XGrabButton(g_display, bind->button,
                    bind->modifiers | modifiers[j],
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

void MouseManager::mouse_function_move(XMotionEvent* me) {
    int x_diff = me->x_root - buttonDragStart_.x;
    int y_diff = me->y_root - buttonDragStart_.y;
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

void MouseManager::mouse_function_resize(XMotionEvent* me) {
    int x_diff = me->x_root - buttonDragStart_.x;
    int y_diff = me->y_root - buttonDragStart_.y;
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

void MouseManager::mouse_function_zoom(XMotionEvent* me) {
    // stretch, where center stays at the same position
    int x_diff = me->x_root - buttonDragStart_.x;
    int y_diff = me->y_root - buttonDragStart_.y;
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
