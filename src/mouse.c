/** Copyright 2011 Thorsten Wißmann. All rights reserved.
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
static HSClient*        g_win_drag_client = 0;
static HSMonitor*       g_drag_monitor = NULL;
static MouseBinding*    g_drag_bind = NULL;

static GList* g_mouse_binds = NULL;
static unsigned int* g_numlockmask_ptr;
#define CLEANMASK(mask)         ((mask) & ~(*g_numlockmask_ptr|LockMask))
#define REMOVEBUTTONMASK(mask) ((mask) & \
    ~( Button1Mask \
     | Button2Mask \
     | Button3Mask \
     | Button4Mask \
     | Button5Mask ))


void mouse_init() {
    g_numlockmask_ptr = get_numlockmask_ptr();
}

void mouse_destroy() {
    mouse_unbind_all();
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

void mouse_stop_drag(XEvent* ev) {
    g_win_drag_client = NULL;
    g_drag_bind = NULL;
    XUngrabPointer(g_display, CurrentTime);
}

void handle_motion_event(XEvent* ev) {
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
    client_resize_floating(g_win_drag_client, g_drag_monitor);
}

void mouse_function_resize(XMotionEvent* me) {
    int x_diff = me->x_root - g_button_drag_start.x_root;
    int y_diff = me->y_root - g_button_drag_start.y_root;
    g_win_drag_client->float_size = g_win_drag_start;
    g_win_drag_client->float_size.width += x_diff;
    g_win_drag_client->float_size.height += y_diff;
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
    g_win_drag_client->float_size = g_win_drag_start;
    g_win_drag_client->float_size.x -= x_diff;
    g_win_drag_client->float_size.y -= y_diff;
    g_win_drag_client->float_size.width += 2 * x_diff;
    g_win_drag_client->float_size.height += 2 * y_diff;
    client_resize_floating(g_win_drag_client, g_drag_monitor);
}


