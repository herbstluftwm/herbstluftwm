#include "mouse.h"

#include <X11/X.h>
#include <cstdlib>
#include <cstring>

#include "client.h"
#include "command.h"
#include "frametree.h"
#include "globals.h"
#include "ipc-protocol.h"
#include "keycombo.h"
#include "keymanager.h"
#include "layout.h"
#include "monitor.h"
#include "mousemanager.h"
#include "root.h"
#include "settings.h"
#include "tag.h"
#include "utils.h"
#include "x11-utils.h"

using std::string;
using std::vector;

static Point2D          g_button_drag_start;
static Rectangle        g_win_drag_start;
static Client*        g_win_drag_client = nullptr;
static Monitor*       g_drag_monitor = nullptr;
static MouseDragFunction g_drag_function = nullptr;

#define CLEANMASK(mask)         ((mask) & ~(numlockMask|LockMask))
#define REMOVEBUTTONMASK(mask) ((mask) & \
    ~( Button1Mask \
     | Button2Mask \
     | Button3Mask \
     | Button4Mask \
     | Button5Mask ))

void mouse_handle_event(XEvent* ev) {
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
    b->action(client, b->cmd);
}

void mouse_initiate_move(Client* client, const vector<string> &cmd) {
    (void) cmd;
    mouse_initiate_drag(client, mouse_function_move);
}

void mouse_initiate_zoom(Client* client, const vector<string> &cmd) {
    (void) cmd;
    mouse_initiate_drag(client, mouse_function_zoom);
}

void mouse_initiate_resize(Client* client, const vector<string> &cmd) {
    (void) cmd;
    mouse_initiate_drag(client, mouse_function_resize);
}

void mouse_call_command(Client* client, const vector<string> &cmd) {
    // TODO: add completion
    client->set_dragged(true);

    // Execute the bound command
    std::ostringstream discardedOutput;
    Input input(cmd.front(), {cmd.begin() + 1, cmd.end()});
    Commands::call(input, discardedOutput);

    client->set_dragged(false);
}


void mouse_initiate_drag(Client* client, MouseDragFunction function) {
    g_drag_function = function;
    g_win_drag_client = client;
    g_drag_monitor = find_monitor_with_tag(client->tag());
    if (!g_drag_monitor || client->tag()->floating == false) {
        // only can drag wins in  floating mode
        g_win_drag_client = nullptr;
        g_drag_function = nullptr;
        return;
    }
    g_win_drag_client->set_dragged( true);
    g_win_drag_start = g_win_drag_client->float_size_;
    g_button_drag_start = get_cursor_position();
    XGrabPointer(g_display, client->x11Window(), True,
        PointerMotionMask|ButtonReleaseMask, GrabModeAsync,
            GrabModeAsync, None, None, CurrentTime);
}

void mouse_stop_drag() {
    if (g_win_drag_client) {
        g_win_drag_client->set_dragged(false);
        // resend last size
        g_drag_monitor->applyLayout();
    }
    g_win_drag_client = nullptr;
    g_drag_function = nullptr;
    XUngrabPointer(g_display, CurrentTime);
    // remove all enternotify-events from the event queue that were
    // generated by the XUngrabPointer
    XEvent ev;
    XSync(g_display, False);
    while(XCheckMaskEvent(g_display, EnterWindowMask, &ev));
}

void handle_motion_event(XEvent* ev) {
    if (!g_drag_monitor) { return; }
    if (!g_win_drag_client) return;
    if (!g_drag_function) return;
    if (ev->type != MotionNotify) return;
    // get newest motion notification
    while (XCheckMaskEvent(g_display, ButtonMotionMask, ev));
    // call function that handles it
    g_drag_function(&(ev->xmotion));
}

bool mouse_is_dragging() {
    return g_drag_function != nullptr;
}

int mouse_unbind_all() {
    Root::get()->mouse->binds.clear();
    Client* client = get_current_client();
    if (client) {
        grab_client_buttons(client, true);
    }
    return 0;
}

int mouse_binding_equals(const MouseBinding* a, const MouseBinding* b) {
    unsigned int numlockMask = Root::get()->keys()->getNumlockMask();
    if((REMOVEBUTTONMASK(CLEANMASK(a->modifiers))
        == REMOVEBUTTONMASK(CLEANMASK(b->modifiers)))
        && (a->button == b->button)) {
        return 0;
    } else {
        return -1;
    }
}

int mouse_bind_command(int argc, char** argv, Output output) {
    if (argc < 3) {
        return HERBST_NEED_MORE_ARGS;
    }
    unsigned int modifiers = 0;
    char* str = argv[1];

    try {
        auto tokens = KeyCombo::tokensFromString(str);
        auto modifierSlice = vector<string>({tokens.begin(), tokens.end() - 1});
        modifiers = KeyCombo::modifierMaskFromTokens(modifierSlice);
    } catch (std::runtime_error &error) {
        output << argv[0] << error.what();
        return HERBST_INVALID_ARGUMENT;
    }

    // last one is the mouse button
    const char* last_token = strlasttoken(str, KeyCombo::separators);
    unsigned int button = string2button(last_token);
    if (button == 0) {
        output << argv[0] <<
            ": Unknown mouse button \"" << last_token << "\"\n";
        return HERBST_INVALID_ARGUMENT;
    }
    MouseFunction function = string2mousefunction(argv[2]);
    if (!function) {
        output << argv[0] <<
            ": Unknown mouse action \"" << argv[2] << "\"\n";
        return HERBST_INVALID_ARGUMENT;
    }

    // actually create a binding
    MouseBinding mb;
    mb.button = button;
    mb.modifiers = modifiers;
    mb.action = function;
    for (int i = 3; i < argc; i++) {
        mb.cmd.push_back(argv[i]);
    }
    Root::get()->mouse->binds.push_front(mb);
    Client* client = get_current_client();
    if (client) {
        grab_client_buttons(client, true);
    }
    return 0;
}

MouseFunction string2mousefunction(char* name) {
    static struct {
        const char* name;
        MouseFunction function;
    } table[] = {
        { "move",       mouse_initiate_move },
        { "zoom",       mouse_initiate_zoom },
        { "resize",     mouse_initiate_resize },
        { "call",       mouse_call_command },
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
unsigned int string2button(const char* name) {
    for (int i = 0; i < LENGTH(string2button_table); i++) {
        if (!strcmp(string2button_table[i].name, name)) {
            return string2button_table[i].button;
        }
    }
    return 0;
}


void complete_against_mouse_buttons(const char* needle, char* prefix, Output output) {
    for (int i = 0; i < LENGTH(string2button_table); i++) {
        const char* buttonname = string2button_table[i].name;
        try_complete_prefix(needle, buttonname, prefix, output);
    }
}

std::experimental::optional<MouseBinding> mouse_binding_find(unsigned int modifiers, unsigned int button) {
    MouseBinding mb = {};
    mb.modifiers = modifiers;
    mb.button = button;

    auto binds = Root::get()->mouse->binds;
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

static void grab_client_button(MouseBinding* bind, Client* client) {
    unsigned int numlockMask = Root::get()->keys()->getNumlockMask();
    unsigned int modifiers[] = { 0, LockMask, numlockMask, numlockMask | LockMask };
    for(int j = 0; j < LENGTH(modifiers); j++) {
        XGrabButton(g_display, bind->button,
                    bind->modifiers | modifiers[j],
                    client->x11Window(), False, ButtonPressMask | ButtonReleaseMask,
                    GrabModeAsync, GrabModeSync, None, None);
    }
}

void grab_client_buttons(Client* client, bool focused) {
    XUngrabButton(g_display, AnyButton, AnyModifier, client->x11Window());
    if (focused) {
        for (auto& bind : Root::get()->mouse->binds) {
            grab_client_button(&bind, client);
        }
    }
    unsigned int btns[] = { Button1, Button2, Button3 };
    for (int i = 0; i < LENGTH(btns); i++) {
        XGrabButton(g_display, btns[i], AnyModifier, client->x11Window(), False,
                    ButtonPressMask|ButtonReleaseMask, GrabModeSync,
                    GrabModeSync, None, None);
    }
}

void mouse_function_move(XMotionEvent* me) {
    int x_diff = me->x_root - g_button_drag_start.x;
    int y_diff = me->y_root - g_button_drag_start.y;
    g_win_drag_client->float_size_ = g_win_drag_start;
    g_win_drag_client->float_size_.x += x_diff;
    g_win_drag_client->float_size_.y += y_diff;
    // snap it to other windows
    int dx, dy;
    client_snap_vector(g_win_drag_client, g_drag_monitor,
                       SNAP_EDGE_ALL, &dx, &dy);
    g_win_drag_client->float_size_.x += dx;
    g_win_drag_client->float_size_.y += dy;
    g_win_drag_client->resize_floating(g_drag_monitor, get_current_client() == g_win_drag_client);
}

void mouse_function_resize(XMotionEvent* me) {
    int x_diff = me->x_root - g_button_drag_start.x;
    int y_diff = me->y_root - g_button_drag_start.y;
    g_win_drag_client->float_size_ = g_win_drag_start;
    // relative x/y coords in drag window
    Monitor* m = g_drag_monitor;
    int rel_x = m->relativeX(g_button_drag_start.x) - g_win_drag_start.x;
    int rel_y = m->relativeY(g_button_drag_start.y) - g_win_drag_start.y;
    bool top = false;
    bool left = false;
    if (rel_y < g_win_drag_start.height/2) {
        top = true;
        y_diff *= -1;
    }
    if (rel_x < g_win_drag_start.width/2) {
        left = true;
        x_diff *= -1;
    }
    // avoid an overflow
    int new_width  = g_win_drag_client->float_size_.width + x_diff;
    int new_height = g_win_drag_client->float_size_.height + y_diff;
    Client* client = g_win_drag_client;
    if (left)   g_win_drag_client->float_size_.x -= x_diff;
    if (top)    g_win_drag_client->float_size_.y -= y_diff;
    g_win_drag_client->float_size_.width  = new_width;
    g_win_drag_client->float_size_.height = new_height;
    // snap it to other windows
    int dx, dy;
    int snap_flags = 0;
    if (left)   snap_flags |= SNAP_EDGE_LEFT;
    else        snap_flags |= SNAP_EDGE_RIGHT;
    if (top)    snap_flags |= SNAP_EDGE_TOP;
    else        snap_flags |= SNAP_EDGE_BOTTOM;
    client_snap_vector(g_win_drag_client, g_drag_monitor,
                       (SnapFlags)snap_flags, &dx, &dy);
    if (left) {
        g_win_drag_client->float_size_.x += dx;
        dx *= -1;
    }
    if (top) {
        g_win_drag_client->float_size_.y += dy;
        dy *= -1;
    }
    g_win_drag_client->float_size_.width += dx;
    g_win_drag_client->float_size_.height += dy;
    client->applysizehints(&g_win_drag_client->float_size_.width,
                           &g_win_drag_client->float_size_.height);
    if (left) {
        client->float_size_.x =
            g_win_drag_start.x + g_win_drag_start.width
            - g_win_drag_client->float_size_.width;
    }
    if (top) {
        client->float_size_.y =
            g_win_drag_start.y + g_win_drag_start.height
            - g_win_drag_client->float_size_.height;
    }
    g_win_drag_client->resize_floating(g_drag_monitor, get_current_client() == g_win_drag_client);
}

void mouse_function_zoom(XMotionEvent* me) {
    // stretch, where center stays at the same position
    int x_diff = me->x_root - g_button_drag_start.x;
    int y_diff = me->y_root - g_button_drag_start.y;
    // relative x/y coords in drag window
    Monitor* m = g_drag_monitor;
    int rel_x = m->relativeX(g_button_drag_start.x) - g_win_drag_start.x;
    int rel_y = m->relativeY(g_button_drag_start.y) - g_win_drag_start.y;
    int cent_x = g_win_drag_start.x + g_win_drag_start.width  / 2;
    int cent_y = g_win_drag_start.y + g_win_drag_start.height / 2;
    if (rel_x < g_win_drag_start.width/2) {
        x_diff *= -1;
    }
    if (rel_y < g_win_drag_start.height/2) {
        y_diff *= -1;
    }
    Client* client = g_win_drag_client;

    // avoid an overflow
    int new_width  = g_win_drag_start.width  + 2 * x_diff;
    int new_height = g_win_drag_start.height + 2 * y_diff;
    // apply new rect
    client->float_size_.x = cent_x - new_width / 2;
    client->float_size_.y = cent_y - new_height / 2;
    client->float_size_.width = new_width;
    client->float_size_.height = new_height;
    // snap it to other windows
    int right_dx, bottom_dy;
    int left_dx, top_dy;
    // we have to distinguish the direction in which we zoom
    client_snap_vector(g_win_drag_client, m,
                     (SnapFlags)(SNAP_EDGE_BOTTOM | SNAP_EDGE_RIGHT), &right_dx, &bottom_dy);
    client_snap_vector(g_win_drag_client, m,
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
    g_win_drag_client->resize_floating(g_drag_monitor, get_current_client() == g_win_drag_client);
}

struct SnapData {
    Client*       client;
    Rectangle      rect;
    enum SnapFlags  flags;
    int             dx, dy; // the vector from client to other to make them snap
};

bool is_point_between(int point, int left, int right) {
    return (point < right && point >= left);
}

// compute vector to snap a point to an edge
static void snap_1d(int x, int edge, int* delta) {
    // whats the vector from subject to edge?
    int cur_delta = edge - x;
    // if distance is smaller then all other deltas
    if (abs(cur_delta) < abs(*delta)) {
        // then snap it, i.e. save vector
        *delta = cur_delta;
    }
}

static void client_snap_helper(Client* candidate, struct SnapData* d) {
    if (candidate == d->client) {
        return;
    }
    auto subject  = d->rect;
    auto other    = candidate->dec.last_outer();
    // increase other by snap gap
    other.x -= g_settings->snap_gap();
    other.y -= g_settings->snap_gap();
    other.width += g_settings->snap_gap() * 2;
    other.height += g_settings->snap_gap() * 2;
    if (intervals_intersect(other.y, other.y + other.height, subject.y, subject.y + subject.height)) {
        // check if x can snap to the right
        if (d->flags & SNAP_EDGE_RIGHT) {
            snap_1d(subject.x + subject.width, other.x, &d->dx);
        }
        // or to the left
        if (d->flags & SNAP_EDGE_LEFT) {
            snap_1d(subject.x, other.x + other.width, &d->dx);
        }
    }
    if (intervals_intersect(other.x, other.x + other.width, subject.x, subject.x + subject.width)) {
        // if we can snap to the top
        if (d->flags & SNAP_EDGE_TOP) {
            snap_1d(subject.y, other.y + other.height, &d->dy);
        }
        // or to the bottom
        if (d->flags & SNAP_EDGE_BOTTOM) {
            snap_1d(subject.y + subject.height, other.y, &d->dy);
        }
    }
    return;
}

// get the vector to snap a client to it's neighbour
void client_snap_vector(Client* client, Monitor* monitor,
                        enum SnapFlags flags,
                        int* return_dx, int* return_dy) {
    struct SnapData d;
    HSTag* tag = monitor->tag;
    int distance = std::max(0, g_settings->snap_distance());
    // init delta
    *return_dx = 0;
    *return_dy = 0;
    if (!distance) {
        // nothing to do
        return;
    }
    d.client    = client;
    // translate client rectangle to global coordinates
    d.rect      = client->outer_floating_rect();
    d.rect.x += monitor->rect.x + monitor->pad_left;
    d.rect.y += monitor->rect.y + monitor->pad_up;
    d.flags     = flags;
    d.dx        = distance;
    d.dy        = distance;

    // snap to monitor edges
    Monitor* m = g_drag_monitor;
    if (flags & SNAP_EDGE_TOP) {
        snap_1d(d.rect.y, m->rect.y + m->pad_up + g_settings->snap_gap(), &d.dy);
    }
    if (flags & SNAP_EDGE_LEFT) {
        snap_1d(d.rect.x, m->rect.x + m->pad_left + g_settings->snap_gap(), &d.dx);
    }
    if (flags & SNAP_EDGE_RIGHT) {
        snap_1d(d.rect.x + d.rect.width, m->rect.x + m->rect.width - m->pad_right - g_settings->snap_gap(), &d.dx);
    }
    if (flags & SNAP_EDGE_BOTTOM) {
        snap_1d(d.rect.y + d.rect.height, m->rect.y + m->rect.height - m->pad_down - g_settings->snap_gap(), &d.dy);
    }

    // snap to other clients
    tag->frame->root_->foreachClient([&d] (Client* c) { client_snap_helper(c, &d); });

    // write back results
    if (abs(d.dx) < abs(distance)) {
        *return_dx = d.dx;
    }
    if (abs(d.dy) < abs(distance)) {
        *return_dy = d.dy;
    }
}

