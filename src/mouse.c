/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "mouse.h"
#include "globals.h"
#include "clientlist.h"
#include "layout.h"
#include "key.h"
#include "ipc-protocol.h"
#include "utils.h"
#include "settings.h"


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>

// gui
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>


static XButtonPressedEvent g_button_drag_start;
static XRectangle       g_win_drag_start;
static HSClient*        g_win_drag_client = NULL;
static HSMonitor*       g_drag_monitor = NULL;
static MouseBinding*    g_drag_bind = NULL;

static Cursor g_cursor;
static GList* g_mouse_binds = NULL;
static unsigned int* g_numlockmask_ptr;
static int* g_snap_distance;
static int* g_snap_gap;

#define CLEANMASK(mask)         ((mask) & ~(*g_numlockmask_ptr|LockMask))
#define REMOVEBUTTONMASK(mask) ((mask) & \
    ~( Button1Mask \
     | Button2Mask \
     | Button3Mask \
     | Button4Mask \
     | Button5Mask ))

void mouse_init() {
    g_numlockmask_ptr = get_numlockmask_ptr();
    g_snap_distance = &(settings_find("snap_distance")->value.i);
    g_snap_gap = &(settings_find("snap_gap")->value.i);
    /* set cursor theme */
    g_cursor = XCreateFontCursor(g_display, XC_left_ptr);
    XDefineCursor(g_display, g_root, g_cursor);
}

void mouse_destroy() {
    mouse_unbind_all();
    XFreeCursor(g_display, g_cursor);
}

void mouse_start_drag(XEvent* ev) {
    XButtonEvent* be = &(ev->xbutton);
    g_drag_bind = mouse_binding_find(be->state, be->button);
    if (!g_drag_bind) {
        // there is no valid bind for this type of mouse event
        return;
    }
    Window win = ev->xbutton.subwindow;
    g_win_drag_client = get_client_from_window(win);
    if (!g_win_drag_client) {
        g_drag_bind = NULL;
        return;
    }
    if (g_win_drag_client->tag->floating == false) {
        // only can drag wins in  floating mode
        g_win_drag_client = NULL;
        g_drag_bind = NULL;
        return;
    }
    g_win_drag_start = g_win_drag_client->float_size;
    g_button_drag_start = ev->xbutton;
    g_drag_monitor = get_current_monitor();
    XGrabPointer(g_display, win, True,
        PointerMotionMask|ButtonReleaseMask, GrabModeAsync,
            GrabModeAsync, None, None, CurrentTime);
}

void mouse_stop_drag() {
    g_win_drag_client = NULL;
    g_drag_bind = NULL;
    XUngrabPointer(g_display, CurrentTime);
}

void handle_motion_event(XEvent* ev) {
    if (g_drag_monitor != get_current_monitor()) {
        mouse_stop_drag();
        return;
    }
    if (!g_win_drag_client) return;
    if (!g_drag_bind) return;
    if (ev->type != MotionNotify) return;
    MouseFunction function = g_drag_bind->function;
    if (!function) return;
    // call function that handles it
    function(&(ev->xmotion));
}

static void grab_button(MouseBinding* mb) {
    unsigned int numlockmask = *g_numlockmask_ptr;
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    // grab button for each modifier that is ignored (capslock, numlock)
    for (int i = 0; i < LENGTH(modifiers); i++) {
        XGrabButton(g_display, mb->button, modifiers[i]|mb->modifiers,
                    g_root, True, ButtonPressMask,
                    GrabModeAsync, GrabModeAsync, None, None);
    }
}

void mouse_regrab_all() {
    update_numlockmask();
    // init modifiers after updating numlockmask
    XUngrabButton(g_display, AnyButton, AnyModifier, g_root);
    g_list_foreach(g_mouse_binds, (GFunc)grab_button, NULL);
}

void mouse_bind_function(unsigned int modifiers, unsigned int button,
                         MouseFunction function) {
    MouseBinding* mb = g_new(MouseBinding, 1);
    mb->button = button;
    mb->modifiers = modifiers;
    mb->function = function;
    g_mouse_binds = g_list_prepend(g_mouse_binds, mb);
    grab_button(mb);
}

int mouse_unbind_all() {
#if GLIB_CHECK_VERSION(2, 28, 0)
    g_list_free_full(g_mouse_binds, g_free); // only available since glib 2.28
#else
    // actually this is not c-standard-compatible because of casting
    // an one-parameter-function to an 2-parameter-function.
    // but it should work on almost all architectures (maybe not amd64?)
    g_list_foreach(g_mouse_binds, (GFunc)g_free, 0);
    g_list_free(g_mouse_binds);
#endif
    g_mouse_binds = NULL;
    XUngrabButton(g_display, AnyButton, AnyModifier, g_root);
    return 0;
}

int mouse_binding_equals(MouseBinding* a, MouseBinding* b) {
    if((REMOVEBUTTONMASK(CLEANMASK(a->modifiers))
        == REMOVEBUTTONMASK(CLEANMASK(b->modifiers)))
        && (a->button == b->button)) {
        return 0;
    } else {
        return -1;
    }
}

int mouse_bind_command(int argc, char** argv) {
    if (argc < 3) {
        return HERBST_INVALID_ARGUMENT;
    }
    unsigned int modifiers = 0;
    char* string = argv[1];
    if (!string2modifiers(string, &modifiers)) {
        return HERBST_INVALID_ARGUMENT;
    }
    // last one is the mouse button
    char* last_token = strlasttoken(string, KEY_COMBI_SEPARATORS);
    unsigned int button = string2button(last_token);
    if (button == 0) {
        fprintf(stderr, "warning: unknown mouse button \"%s\"\n", last_token);
        return HERBST_INVALID_ARGUMENT;
    }
    MouseFunction function = string2mousefunction(argv[2]);
    if (!function) {
        fprintf(stderr, "warning: unknown mouse action \"%s\"\n", argv[2]);
        return HERBST_INVALID_ARGUMENT;
    }
    mouse_bind_function(modifiers, button, function);
    return 0;
}

MouseFunction string2mousefunction(char* name) {
    static struct {
        char* name;
        MouseFunction function;
    } table[] = {
        { "move",       mouse_function_move },
        { "zoom",       mouse_function_zoom },
        { "resize",     mouse_function_resize },
    };
    int i;
    for (i = 0; i < LENGTH(table); i++) {
        if (!strcmp(table[i].name, name)) {
            return table[i].function;
        }
    }
    return NULL;
}

unsigned int string2button(char* name) {
    static struct {
        char* name;
        unsigned int button;
    } table[] = {
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
    int i;
    for (i = 0; i < LENGTH(table); i++) {
        if (!strcmp(table[i].name, name)) {
            return table[i].button;
        }
    }
    return 0;
}

MouseBinding* mouse_binding_find(unsigned int modifiers, unsigned int button) {
    MouseBinding mb = { .modifiers = modifiers, .button = button, 0};
    GList* elem = g_list_find_custom(g_mouse_binds, &mb,
                                     (GCompareFunc)mouse_binding_equals);
    return elem ? elem->data : NULL;
}

void mouse_function_move(XMotionEvent* me) {
    int x_diff = me->x_root - g_button_drag_start.x_root;
    int y_diff = me->y_root - g_button_drag_start.y_root;
    g_win_drag_client->float_size = g_win_drag_start;
    g_win_drag_client->float_size.x += x_diff;
    g_win_drag_client->float_size.y += y_diff;
    // snap it to other windows
    int dx, dy;
    client_snap_vector(g_win_drag_client, g_win_drag_client->tag,
                       SNAP_EDGE_ALL, &dx, &dy);
    g_win_drag_client->float_size.x += dx;
    g_win_drag_client->float_size.y += dy;
    client_resize_floating(g_win_drag_client, g_drag_monitor);
}

void mouse_function_resize(XMotionEvent* me) {
    int x_diff = me->x_root - g_button_drag_start.x_root;
    int y_diff = me->y_root - g_button_drag_start.y_root;
    g_win_drag_client->float_size = g_win_drag_start;
    // relative x/y coords in drag window
    HSMonitor* m = g_drag_monitor;
    int rel_x = monitor_get_relative_x(m, g_button_drag_start.x_root) - g_win_drag_start.x;
    int rel_y = monitor_get_relative_y(m, g_button_drag_start.y_root) - g_win_drag_start.y;
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
    int new_width  = g_win_drag_client->float_size.width + x_diff;
    int new_height = g_win_drag_client->float_size.height + y_diff;
    if (left)   g_win_drag_client->float_size.x -= x_diff;
    if (top)    g_win_drag_client->float_size.y -= y_diff;
    if (new_width <  WINDOW_MIN_WIDTH)  new_width = WINDOW_MIN_WIDTH;
    if (new_height < WINDOW_MIN_HEIGHT) new_height = WINDOW_MIN_HEIGHT;
    g_win_drag_client->float_size.width  = new_width;
    g_win_drag_client->float_size.height = new_height;
    // snap it to other windows
    int dx, dy;
    int snap_flags = 0;
    if (left)   snap_flags |= SNAP_EDGE_LEFT;
    else        snap_flags |= SNAP_EDGE_RIGHT;
    if (top)    snap_flags |= SNAP_EDGE_TOP;
    else        snap_flags |= SNAP_EDGE_BOTTOM;
    client_snap_vector(g_win_drag_client, g_win_drag_client->tag,
                       snap_flags, &dx, &dy);
    if (left) {
        g_win_drag_client->float_size.x += dx;
        dx *= -1;
    }
    if (top) {
        g_win_drag_client->float_size.y += dy;
        dy *= -1;
    }
    g_win_drag_client->float_size.width += dx;
    g_win_drag_client->float_size.height += dy;
    client_resize_floating(g_win_drag_client, g_drag_monitor);
}

void mouse_function_zoom(XMotionEvent* me) {
    // scretch, where center stays at the same position
    int x_diff = me->x_root - g_button_drag_start.x_root;
    int y_diff = me->y_root - g_button_drag_start.y_root;
    // relative x/y coords in drag window
    HSMonitor* m = g_drag_monitor;
    int rel_x = monitor_get_relative_x(m, g_button_drag_start.x_root) - g_win_drag_start.x;
    int rel_y = monitor_get_relative_y(m, g_button_drag_start.y_root) - g_win_drag_start.y;
    if (rel_x < g_win_drag_start.width/2) {
        x_diff *= -1;
    }
    if (rel_y < g_win_drag_start.height/2) {
        y_diff *= -1;
    }

    // avoid an overflow
    int new_width  = g_win_drag_start.width  + 2 * x_diff;
    int new_height = g_win_drag_start.height + 2 * y_diff;
    if (new_width < WINDOW_MIN_WIDTH) {
        int overflow = WINDOW_MIN_WIDTH - new_width;
        overflow += overflow % 2; // make odd overflow even
        x_diff += overflow;
    }
    if (new_height < WINDOW_MIN_HEIGHT) {
        int overflow = WINDOW_MIN_HEIGHT - new_height;
        overflow += overflow % 2; // make odd overflow even
        y_diff += overflow;
    }

    // apply new rect
    g_win_drag_client->float_size = g_win_drag_start;
    g_win_drag_client->float_size.x -= x_diff;
    g_win_drag_client->float_size.y -= y_diff;
    g_win_drag_client->float_size.width += 2 * x_diff;
    g_win_drag_client->float_size.height += 2 * y_diff;
    // snap it to other windows
    int right_dx, bottom_dy;
    int left_dx, top_dy;
    // we have to distinguish the direction in which we zoom
    client_snap_vector(g_win_drag_client, g_win_drag_client->tag,
                     SNAP_EDGE_BOTTOM | SNAP_EDGE_RIGHT, &right_dx, &bottom_dy);
    client_snap_vector(g_win_drag_client, g_win_drag_client->tag,
                       SNAP_EDGE_TOP | SNAP_EDGE_LEFT, &left_dx, &top_dy);
    // e.g. if window snaps by vector (3,3) at topleft, window has to be shrinked
    // but if the window snaps by vector (3,3) at bottomright, window has to grow
    if (abs(right_dx) < abs(left_dx)) {
        right_dx = -left_dx;
    }
    if (abs(bottom_dy) < abs(top_dy)) {
        bottom_dy = -top_dy;
    }
    g_win_drag_client->float_size.width += 2 * right_dx;
    g_win_drag_client->float_size.x     -= right_dx;
    g_win_drag_client->float_size.height += 2 * bottom_dy;
    g_win_drag_client->float_size.y     -= bottom_dy;
    client_resize_floating(g_win_drag_client, g_drag_monitor);
}

struct SnapData {
    HSClient*       client;
    XRectangle      rect;
    enum SnapFlags  flags;
    int             dx, dy; // the vector from client to other to make them snap
};

static bool is_point_between(int point, int left, int right) {
    return (point < right && point >= left);
}

static bool intervals_intersect(int a_left, int a_right, int b_left, int b_right) {
    return is_point_between(a_left, b_left, b_right)
        || is_point_between(a_right, b_left, b_right)
        || is_point_between(b_right, a_left, a_right)
        || is_point_between(b_left, a_left, a_right);
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

static int client_snap_helper(HSClient* candidate, struct SnapData* d) {
    if (candidate == d->client) {
        return 0;
    }
    XRectangle subject  = d->rect;
    XRectangle other    = client_outer_floating_rect(candidate);
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
    return 0;
}

// get the vector to snap a client to it's neighbour
void client_snap_vector(struct HSClient* client, struct HSTag* tag,
                        enum SnapFlags flags,
                        int* return_dx, int* return_dy) {
    struct SnapData d;
    int distance = (*g_snap_distance > 0) ? *g_snap_distance : 0;
    // init delta
    *return_dx = 0;
    *return_dy = 0;
    if (!distance) {
        // nothing to do
        return;
    }
    d.client    = client;
    d.rect      = client_outer_floating_rect(client);
    d.flags     = flags;
    d.dx        = distance;
    d.dy        = distance;

    // snap to monitor edges
    HSMonitor* m = g_drag_monitor;
    if (flags & SNAP_EDGE_TOP) {
        snap_1d(d.rect.y, *g_snap_gap, &d.dy);
    }
    if (flags & SNAP_EDGE_LEFT) {
        snap_1d(d.rect.x, *g_snap_gap, &d.dx);
    }
    if (flags & SNAP_EDGE_RIGHT) {
        snap_1d(d.rect.x + d.rect.width, m->rect.width - m->pad_left - m->pad_right, &d.dx);
    }
    if (flags & SNAP_EDGE_BOTTOM) {
        snap_1d(d.rect.y + d.rect.height, m->rect.height - m->pad_up - m->pad_down, &d.dy);
    }

    // snap to other clients
    frame_foreach_client(tag->frame, (ClientAction)client_snap_helper, &d);

    // write back results
    if (abs(d.dx) < abs(distance)) {
        *return_dx = d.dx;
    }
    if (abs(d.dy) < abs(distance)) {
        *return_dy = d.dy;
    }
}

