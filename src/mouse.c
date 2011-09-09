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


static int  g_win_drag_x;
static int  g_win_drag_y;
static XRectangle g_win_drag_start;
static HSClient* g_win_drag_client = 0;

static GList* g_mouse_binds = NULL;


void mouse_init() {
}

void mouse_destroy() {
    mouse_unbind_all();
}

void mouse_start_drag(XEvent* ev) {
    g_win_drag_x = ev->xbutton.x_root;
    g_win_drag_y = ev->xbutton.y_root;
    Window win = ev->xbutton.subwindow;
    g_win_drag_client = get_client_from_window(win);
    if (!g_win_drag_client) {
        printf("unknown win %lx, root = %lx\n", win, g_root);
        return;
    } else {
        printf("win %lx\n", win);
    }
    if (g_win_drag_client->tag->floating == false) {
        // only can drag wins in  floating mode
        g_win_drag_client = NULL;
        return;
    }
    g_win_drag_start = g_win_drag_client->float_size;

    XGrabPointer(g_display, win, True,
        PointerMotionMask|ButtonReleaseMask, GrabModeAsync,
            GrabModeAsync, None, None, CurrentTime);
}

void mouse_stop_drag(XEvent* ev) {
    g_win_drag_client = NULL;
    XUngrabPointer(g_display, CurrentTime);
}

void handle_motion_event(XEvent* ev) {
    if (!g_win_drag_client) return;
    HSMonitor* m = get_current_monitor();
    int horiz_diff = ev->xmotion.x_root - g_win_drag_x;
    int vert_diff = ev->xmotion.y_root - g_win_drag_y;
    bool pos = ev->xmotion.state & Button1Mask;
    bool size = ev->xmotion.state & Button3Mask;
    int xdiff = pos ? horiz_diff : 0;
    int ydiff = pos ? vert_diff  : 0;
    int wdiff = size ? horiz_diff : 0;
    int hdiff = size ? vert_diff  : 0;
    if (pos && size) {
        wdiff *= -1;
        hdiff *= -1;
    }
    g_win_drag_client->float_size = g_win_drag_start;
    g_win_drag_client->float_size.x += xdiff;
    g_win_drag_client->float_size.y += ydiff;
    g_win_drag_client->float_size.width += wdiff;
    g_win_drag_client->float_size.height += hdiff;
    client_resize_floating(g_win_drag_client, m);
}

void mouse_bind_function(unsigned int modifiers, unsigned int button,
                         MouseFunction function) {
    MouseBinding* mb = g_new(MouseBinding, 1);
    mb->button = button;
    mb->modifiers = modifiers;
    mb->function = function;
    g_mouse_binds = g_list_prepend(g_mouse_binds, mb);
    XGrabButton(g_display, mb->button, mb->modifiers, g_root, True,
        ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
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
    if((a->modifiers == b->modifiers) && (a->button == b->button)) {
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
        { "move", handle_motion_event },
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

