
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

// atoms from dwm.c
enum { WMProtocols, WMDelete, WMState, WMLast };        /* default atoms */
enum { NetSupported, NetWMName, NetWMState,
       NetWMFullscreen, NetLast };                      /* EWMH atoms */
static Atom g_wmatom[WMLast], g_netatom[NetLast];


static void fetch_colors() {
    g_window_border_width = &(settings_find("window_border_width")->value.i);
    char* str = settings_find("window_border_normal_color")->value.s;
    g_window_border_normal_color = getcolor(str);
    str = settings_find("window_border_active_color")->value.s;
    g_window_border_active_color = getcolor(str);
}

void clientlist_init() {
    fetch_colors();
    g_wmatom[WMProtocols] = XInternAtom(g_display, "WM_PROTOCOLS", False);
    g_wmatom[WMDelete] = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    g_wmatom[WMState] = XInternAtom(g_display, "WM_STATE", False);
    g_netatom[NetSupported] = XInternAtom(g_display, "_NET_SUPPORTED", False);
    g_netatom[NetWMName] = XInternAtom(g_display, "_NET_WM_NAME", False);
    g_netatom[NetWMState] = XInternAtom(g_display, "_NET_WM_STATE", False);
    g_netatom[NetWMFullscreen] = XInternAtom(g_display, "_NET_WM_STATE_FULLSCREEN", False);
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
    if (rect.width <= WINDOW_MIN_WIDTH || rect.height <= WINDOW_MIN_HEIGHT) {
        // do nothing on invalid size
        return;
    }
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

// from dwm.c
int window_close_current() {
    XEvent ev;
    // if there is no focus, then there is nothing to do
    if (!g_cur_frame) return 0;
    Window win = frame_focused_window(g_cur_frame);
    if (!win) return 0;
    ev.type = ClientMessage;
    ev.xclient.window = win;
    ev.xclient.message_type = g_wmatom[WMProtocols];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = g_wmatom[WMDelete];
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(g_display, win, False, NoEventMask, &ev);
    return 0;
}


