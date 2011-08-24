/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "clientlist.h"
#include "settings.h"
#include "globals.h"
#include "layout.h"
#include "utils.h"
// system
#include <glib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include <stdbool.h>
// gui
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

int* g_window_border_width;
unsigned long g_window_border_active_color;
unsigned long g_window_border_normal_color;
regex_t g_ignore_class_regex; // clients that match this won't be managed

GHashTable* g_clients;

// atoms from dwm.c
enum { WMProtocols, WMDelete, WMState, WMLast };        /* default atoms */
enum { NetSupported, NetWMName, NetWMState,
       NetWMFullscreen, NetLast };                      /* EWMH atoms */
static Atom g_wmatom[WMLast], g_netatom[NetLast];

static HSClient* create_client() {
    HSClient* hc = g_new0(HSClient, 1);
    return hc;
}

void reset_client_settings() {
    // reset regex
    regfree(&g_ignore_class_regex);
    char* str = settings_find("ignore_class")->value.s;
    int status = regcomp(&g_ignore_class_regex, str, REG_NOSUB|REG_EXTENDED);
    if (status != 0) {
        char buf[ERROR_STRING_BUF_SIZE];
        regerror(status, &g_ignore_class_regex, buf, ERROR_STRING_BUF_SIZE);
        fprintf(stderr, "Cannot parse value \"%s\"", str);
        fprintf(stderr, "from setting \"%s\": ", "ignore_class");
        fprintf(stderr, "\"%s\"\n", buf);
    }
}

static void fetch_colors() {
    g_window_border_width = &(settings_find("window_border_width")->value.i);
    char* str = settings_find("window_border_normal_color")->value.s;
    g_window_border_normal_color = getcolor(str);
    str = settings_find("window_border_active_color")->value.s;
    g_window_border_active_color = getcolor(str);
}

void clientlist_init() {
    // init regex simple..
    char* default_regex = settings_find("ignore_class")->value.s;
    assert(0 == regcomp(&g_ignore_class_regex, default_regex,
                        REG_NOSUB|REG_EXTENDED));
    fetch_colors();
    g_wmatom[WMProtocols] = XInternAtom(g_display, "WM_PROTOCOLS", False);
    g_wmatom[WMDelete] = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
    g_wmatom[WMState] = XInternAtom(g_display, "WM_STATE", False);
    g_netatom[NetSupported] = XInternAtom(g_display, "_NET_SUPPORTED", False);
    g_netatom[NetWMName] = XInternAtom(g_display, "_NET_WM_NAME", False);
    g_netatom[NetWMState] = XInternAtom(g_display, "_NET_WM_STATE", False);
    g_netatom[NetWMFullscreen] = XInternAtom(g_display, "_NET_WM_STATE_FULLSCREEN", False);
    // init actual client list
    g_clients = g_hash_table_new_full(g_int_hash, g_int_equal,
                                      NULL, (GDestroyNotify)destroy_client);
}

void reset_client_colors() {
    fetch_colors();
    all_monitors_apply_layout();
}

void clientlist_destroy() {
    g_hash_table_destroy(g_clients);
    regfree(&g_ignore_class_regex);
}

HSClient* get_client_from_window(Window window) {
    return (HSClient*) g_hash_table_lookup(g_clients, &window);
}

static void window_grab_button(Window win) {
    XGrabButton(g_display, AnyButton, AnyModifier, win, true, ButtonPressMask,
                GrabModeSync, GrabModeSync, None, None);
}

void manage_client(Window win) {
    if (is_herbstluft_window(g_display, win)) {
        // ignore our own window
        return;
    }
    if (get_client_from_window(win)) {
        return;
    }
    // init client
    XSetWindowBorderWidth(g_display, win, *g_window_border_width);
    HSClient* client = create_client();
    client->window = win;
    g_hash_table_insert(g_clients, &(client->window), client);
    // insert to layout
    HSMonitor* m = &g_array_index(g_monitors, HSMonitor, g_cur_monitor);
    client->tag = m->tag;
    // get events from window
    XSelectInput(g_display, win, CLIENT_EVENT_MASK);
    window_grab_button(win);
    frame_insert_window(m->tag->frame, win);
    monitor_apply_layout(m);
}

void unmanage_client(Window win) {
    HSClient* client = get_client_from_window(win);
    if (!client) {
        return;
    }
    // remove from tag
    frame_remove_window(client->tag->frame, win);
    // and arrange monitor
    HSMonitor* m = find_monitor_with_tag(client->tag);
    if (m) monitor_apply_layout(m);
    // ignore events from it
    XSelectInput(g_display, win, 0);
    XUngrabButton(g_display, AnyButton, AnyModifier, win);
    // permanently remove it
    g_hash_table_remove(g_clients, &win);
}

// destroys a special client
void destroy_client(HSClient* client) {
    g_free(client);
}

void window_unfocus(Window window) {
    // grab buttons in old window again
    printf("unfocusing %lx\n", window);
    XSetWindowBorder(g_display, window, g_window_border_normal_color);
    window_grab_button(window);
}

static Window lastfocus = 0;
void window_unfocus_last() {
    if (lastfocus) {
        window_unfocus(lastfocus);
        lastfocus = 0;
    }
    // give focus to root window
    XUngrabButton(g_display, AnyButton, AnyModifier, g_root);
    XSetInputFocus(g_display, g_root, RevertToPointerRoot, CurrentTime);
}

void window_focus(Window window) {
    // unfocus last one
    window_unfocus(lastfocus);
    lastfocus = window;
    // change window-colors
    XSetWindowBorder(g_display, window, g_window_border_active_color);
    // set keyboardfocus
    XUngrabButton(g_display, AnyButton, AnyModifier, window);
    XSetInputFocus(g_display, window, RevertToPointerRoot, CurrentTime);
}

void window_resize(Window win, XRectangle rect) {
    if (rect.width <= WINDOW_MIN_WIDTH || rect.height <= WINDOW_MIN_HEIGHT) {
        // do nothing on invalid size
        return;
    }
    struct HSClient* client = get_client_from_window(win);
    // apply border width
    rect.width -= *g_window_border_width * 2;
    rect.height -= *g_window_border_width * 2;
    XSetWindowBorderWidth(g_display, win, *g_window_border_width);
    if (client) {
        if (RECTANGLE_EQUALS(client->last_size, rect)) return;
        client->last_size = rect;
    }
    XMoveResizeWindow(g_display, win, rect.x, rect.y, rect.width, rect.height);
    //// send new size to client
    //// WHY SHOULD I? -> faster? only one call?
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

bool is_window_class_ignored(char* window_class) {
    int status = regexec(&g_ignore_class_regex, window_class, 0, NULL, 0);
    return (status == 0);
}

bool is_window_ignored(Window win) {
    GString* window_class = window_class_to_g_string(g_display, win);
    bool b = is_window_class_ignored(window_class->str);
    g_string_free(window_class, true);
    return b;
}

void window_set_visible(Window win, bool visible) {
    static int (*action[])(Display*,Window) = {
        XUnmapWindow,
        XMapWindow,
    };
    unsigned long event_mask = CLIENT_EVENT_MASK;
    XGrabServer(g_display);
    XSelectInput(g_display, win, event_mask & ~StructureNotifyMask);
    XSelectInput(g_display, g_root, ROOT_EVENT_MASK & ~SubstructureNotifyMask);
    action[visible](g_display, win);
    XSelectInput(g_display, win, event_mask);
    XSelectInput(g_display, g_root, ROOT_EVENT_MASK);
    XUngrabServer(g_display);
}

void window_show(Window win) {
    window_set_visible(win, true);
}

void window_hide(Window win) {
    window_set_visible(win, false);
}
