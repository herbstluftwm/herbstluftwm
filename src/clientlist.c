
#include "clientlist.h"
#include "globals.h"
#include "layout.h"
#include "utils.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
// gui
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

void manage_client(Window win) {
    if (is_herbstluft_window(g_display, win)) {
        // ignore our own window
        return;
    }
    // init client
    XSetWindowBorderWidth(g_display, win, 0);
    // insert to layout
    HSMonitor* m = &g_array_index(g_monitors, HSMonitor, g_cur_monitor);
    frame_insert_window(g_cur_frame, win);
    monitor_apply_layout(m);
}

// destroys a special client
void destroy_client(HerbstClient* client) {
    g_free(client);
}

void free_clients() {
}

void window_focus(Window window) {
    XSetInputFocus(g_display, window, RevertToPointerRoot, CurrentTime);
}

