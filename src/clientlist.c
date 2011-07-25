
#include "clientlist.h"
#include "settings.h"
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

int* g_window_border_width;
unsigned long g_window_border_active_color;
unsigned long g_window_border_normal_color;

static void fetch_colors() {
    g_window_border_width = &(settings_find("window_border_width")->value.i);
    char* str = settings_find("window_border_normal_color")->value.s;
    g_window_border_normal_color = getcolor(str);
    str = settings_find("window_border_active_color")->value.s;
    g_window_border_active_color = getcolor(str);
}

void clientlist_init() {
    fetch_colors();
}

void reset_client_colors() {
    fetch_colors();
    all_monitors_apply_layout();
}

void clientlist_destroy() {
}

void manage_client(Window win) {
    if (is_herbstluft_window(g_display, win)) {
        // ignore our own window
        return;
    }
    // init client
    XSetWindowBorderWidth(g_display, win, *g_window_border_width);
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
    static Window lastfocus = 0;
    // change window-colors
    XSetWindowBorder(g_display, lastfocus, g_window_border_normal_color);
    XSetWindowBorder(g_display, window, g_window_border_active_color);
    lastfocus = window;
    // set keyboardfocus
    printf("focusing window %d\n", (int)window);
    XUngrabButton(g_display, AnyButton, AnyModifier, window);
    XSetInputFocus(g_display, window, RevertToPointerRoot, CurrentTime);
}

void window_resize(Window win, XRectangle rect) {
    // apply border width
    rect.width -= *g_window_border_width * 2;
    rect.height -= *g_window_border_width * 2;
    XSetWindowBorderWidth(g_display, win, *g_window_border_width);
    XMoveWindow(g_display, win, rect.x, rect.y);
    XResizeWindow(g_display, win, rect.width, rect.height);
    //// send new size to client
    //// WHY SHOULD I?
    //XConfigureEvent ce;
    //ce.type = ConfigureNotify;
    //ce.display = g_display;
    //ce.event = win;
    //ce.window = win;
    //ce.x = rect.x;
    //ce.y = rect.y;
    //ce.width = rect.width;
    //ce.height = rect.height;
    //ce.border_width = 0;
    //ce.above = None;
    //ce.override_redirect = False;
    //XSendEvent(g_display, win, False, StructureNotifyMask, (XEvent *)&ce);
}



