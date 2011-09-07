/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "mouse.h"
#include "globals.h"
#include "clientlist.h"
#include "layout.h"



#include <stdio.h>

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




void mouse_init() {
}

void mouse_destroy() {
}

void mouse_grab(Window win) {
    XGrabButton(g_display, 1, Mod1Mask, win, True,
        ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(g_display, 3, Mod1Mask, win, True,
        ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
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
    if (!g_win_drag_client) return;;
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


