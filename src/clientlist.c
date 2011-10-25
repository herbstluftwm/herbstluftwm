/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "clientlist.h"
#include "settings.h"
#include "globals.h"
#include "layout.h"
#include "utils.h"
#include "hook.h"
#include "mouse.h"
#include "rules.h"
#include "ipc-protocol.h"
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

int g_monitor_float_treshold = 24;

int* g_window_border_width;
int* g_raise_on_focus;
int* g_snap_gap;
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
    hc->float_size.width = 100;
    hc->float_size.height = 100;
    hc->urgent = false;
    hc->fullscreen = false;
    return hc;
}

void reset_client_settings() {
    char* str = settings_find("ignore_class")->value.s;
    regex_t new_regex;
    int status = regcomp(&new_regex, str, REG_NOSUB|REG_EXTENDED);
    if (status != 0) {
        char buf[ERROR_STRING_BUF_SIZE];
        regerror(status, &new_regex, buf, ERROR_STRING_BUF_SIZE);
        fprintf(stderr, "Cannot parse value \"%s\"", str);
        fprintf(stderr, "from setting \"%s\": ", "ignore_class");
        fprintf(stderr, "\"%s\"\n", buf);
    } else {
        // reset regex
        regfree(&g_ignore_class_regex);
        g_ignore_class_regex = new_regex;
    }
}

static void fetch_colors() {
    g_window_border_width = &(settings_find("window_border_width")->value.i);
    g_snap_gap = &(settings_find("snap_gap")->value.i);
    g_raise_on_focus = &(settings_find("raise_on_focus")->value.i);
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

static void client_move_to_floatpos(void* key, void* client_void, void* data) {
    (void)key;
    (void)data;
    HSClient* client = client_void;
    if (client) {
        int x = client->float_size.x;
        int y = client->float_size.y;
        unsigned int w = client->float_size.width;
        unsigned int h = client->float_size.height;
        XMoveResizeWindow(g_display, client->window, x, y, w, h);
    }
}

void clientlist_destroy() {
    // move all clients to their original floating position
    g_hash_table_foreach(g_clients, client_move_to_floatpos, NULL);

    g_hash_table_destroy(g_clients);
    regfree(&g_ignore_class_regex);
}


void clientlist_foreach(GHFunc func, gpointer data) {
    g_hash_table_foreach(g_clients, func, data);
}

HSClient* get_client_from_window(Window window) {
    return (HSClient*) g_hash_table_lookup(g_clients, &window);
}

static void window_grab_button(Window win) {
    XGrabButton(g_display, AnyButton, 0, win, true, ButtonPressMask,
                GrabModeSync, GrabModeSync, None, None);
}

HSClient* manage_client(Window win) {
    if (is_herbstluft_window(g_display, win)) {
        // ignore our own window
        return NULL;
    }
    if (get_client_from_window(win)) {
        return NULL;
    }
    // init client
    XSetWindowBorderWidth(g_display, win, *g_window_border_width);
    HSClient* client = create_client();
    client->pid = window_pid(g_display, win);
    HSMonitor* m = get_current_monitor();
    // set to window properties
    client->window = win;
    unsigned int border, depth;
    Window root_win;
    int x, y;
    unsigned int w, h;
    XGetGeometry(g_display, win, &root_win, &x, &y, &w, &h, &border, &depth);
    // treat wanted coordinates as floating coords
    client->float_size.x = x;
    client->float_size.y = y;
    client->float_size.width = w;
    client->float_size.height = h;

    // apply rules
    HSClientChanges changes;
    client_changes_init(&changes);
    rules_apply(client, &changes);
    if (changes.tag_name) {
        client->tag = find_tag(changes.tag_name->str);
    }

    if (!changes.manage) {
        client_changes_free_members(&changes);
        destroy_client(client);
        // map it... just to be sure
        XMapWindow(g_display, win);
        return NULL;
    }

    // actually manage it
    g_hash_table_insert(g_clients, &(client->window), client);
    // insert to layout
    if (!client->tag) {
        client->tag = m->tag;
    }
    // get events from window
    XSelectInput(g_display, win, CLIENT_EVENT_MASK);
    window_grab_button(win);
    //mouse_grab(win);
    frame_insert_window(client->tag->frame, win);
    if (changes.focus) {
        // give focus to window if wanted
        // TODO: make this faster!
        // WARNING: this solution needs O(C + exp(D)) time where W is the count
        // of clients on this tag and D is the depth of the binary layout tree
        frame_focus_window(client->tag->frame, win);
    }

    tag_set_flags_dirty();
    monitor_apply_layout(find_monitor_with_tag(client->tag));
    client_changes_free_members(&changes);

    return client;
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
    tag_set_flags_dirty();
}

// destroys a special client
void destroy_client(HSClient* client) {
    g_free(client);
}

void window_unfocus(Window window) {
    // grab buttons in old window again
    XSetWindowBorder(g_display, window, g_window_border_normal_color);
    window_grab_button(window);
    //mouse_grab(window);
}

static Window lastfocus = 0;
void window_unfocus_last() {
    if (lastfocus) {
        window_unfocus(lastfocus);
        lastfocus = 0;
    }
    // give focus to root window
    XSetInputFocus(g_display, g_root, RevertToPointerRoot, CurrentTime);
}

void window_focus(Window window) {
    // unfocus last one
    window_unfocus(lastfocus);
    lastfocus = window;
    // change window-colors
    XSetWindowBorder(g_display, window, g_window_border_active_color);
    //XUngrabButton(g_display, AnyButton, AnyModifier, window);
    // set keyboardfocus
    XSetInputFocus(g_display, window, RevertToPointerRoot, CurrentTime);
    if (*g_raise_on_focus) {
        XRaiseWindow(g_display, window);
    }
    //mouse_grab(window);
}

void client_setup_border(HSClient* client, bool focused) {
    unsigned long colors[] = {
        g_window_border_normal_color,
        g_window_border_active_color,
    };
    XSetWindowBorder(g_display, client->window, colors[focused ? 1 : 0]);
}

void client_resize_fullscreen(HSClient* client, HSMonitor* m) {
    if (!client || !m) {
        HSDebug("client_resize_fullscreen() got invalid parameters\n");
        return;
    }
    XSetWindowBorderWidth(g_display, client->window, 0);
    client->last_size = m->rect;
    XMoveResizeWindow(g_display, client->window,
                      m->rect.x, m->rect.y, m->rect.width, m->rect.height);

}

void client_resize(HSClient* client, XRectangle rect) {
    // ensure minimum size
    if (rect.width < WINDOW_MIN_WIDTH) {
        rect.width = WINDOW_MIN_WIDTH;
    }
    if (rect.height < WINDOW_MIN_HEIGHT) {
        rect.height = WINDOW_MIN_HEIGHT;
    }
    if (!client) {
        HSDebug("Warning: client_resize(NULL, ...) was called\n");
        return;
    }
    Window win = client->window;
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

void client_resize_tiling(HSClient* client, XRectangle rect) {
    HSMonitor* m;
    if (client->fullscreen && (m = find_monitor_with_tag(client->tag))) {
        client_resize_fullscreen(client, m);
    } else {
        client_resize(client, rect);
    }
}

void client_resize_floating(HSClient* client, HSMonitor* m) {
    if (!client || !m) return;
    if (client->fullscreen) {
        client_resize_fullscreen(client, m);
        return;
    }

    // ensure minimal size
    if (client->float_size.width < WINDOW_MIN_WIDTH)
        client->float_size.width = WINDOW_MIN_WIDTH;
    if (client->float_size.height < WINDOW_MIN_HEIGHT)
        client->float_size.height = WINDOW_MIN_HEIGHT;
    client->last_size = client->float_size;
    client->last_size.x += m->rect.x + m->pad_left;
    client->last_size.y += m->rect.y + m->pad_up;

    // ensure position is on monitor
    int space = g_monitor_float_treshold;
    client->last_size.x =
        CLAMP(client->last_size.x,
              m->rect.x + m->pad_left - client->last_size.width + space,
              m->rect.x + m->rect.width - m->pad_left - m->pad_right - space);
    client->last_size.y =
        CLAMP(client->last_size.y,
              m->rect.y + m->pad_up - client->last_size.height + space,
              m->rect.y + m->rect.height - m->pad_up - m->pad_down - space);
    XRectangle rect = client->last_size;
    XSetWindowBorderWidth(g_display, client->window, *g_window_border_width);
    XMoveResizeWindow(g_display, client->window,
        rect.x, rect.y, rect.width, rect.height);
}

XRectangle client_outer_floating_rect(HSClient* client) {
    XRectangle rect = client->float_size;
    rect.width  += *g_window_border_width * 2 + *g_snap_gap;
    rect.height += *g_window_border_width * 2 + *g_snap_gap;
    return rect;
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

// heavily inspired by dwm.c
void client_clear_urgent(HSClient* client) {
    if (client->urgent) {
        char winid_str[STRING_BUF_SIZE];
        snprintf(winid_str, STRING_BUF_SIZE, "0x%lx", client->window);
        hook_emit_list("urgent", "off", winid_str, NULL);
        client->urgent = false;
        XWMHints *wmh;
        if(!(wmh = XGetWMHints(g_display, client->window)))
            return;
        wmh->flags &= ~XUrgencyHint;
        XSetWMHints(g_display, client->window, wmh);
        XFree(wmh);
        // report changes to tags
        tag_set_flags_dirty();
    }
}

// heavily inspired by dwm.c
void client_update_wm_hints(HSClient* client) {
    XWMHints* wmh = XGetWMHints(g_display, client->window);
    if (!wmh) {
        return;
    }

    if ((frame_focused_window(g_cur_frame) == client->window)
        && wmh->flags & XUrgencyHint) {
        // remove urgency hint if window is focused
        wmh->flags &= ~XUrgencyHint;
        XSetWMHints(g_display, client->window, wmh);
    } else {
        bool newval = (wmh->flags & XUrgencyHint) ? true : false;
        if (newval != client->urgent) {
            client->urgent = newval;
            char winid_str[STRING_BUF_SIZE];
            snprintf(winid_str, STRING_BUF_SIZE, "0x%lx", client->window);
            hook_emit_list("urgent", client->urgent ? "on":"off", winid_str, NULL);
            tag_set_flags_dirty();
        }
    }
}

HSClient* get_current_client() {
    Window win = frame_focused_window(g_cur_frame);
    if (!win) return NULL;
    return get_client_from_window(win);
}

void client_set_fullscreen(HSClient* client, bool state) {
    client->fullscreen = state;
    if (state) {
        // TODO: do proper stacking layer handling
        XRaiseWindow(g_display, client->window);
    }
    monitor_apply_layout(get_current_monitor());
}

int client_set_fullscreen_command(int argc, char** argv) {
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }

    HSClient* client = get_current_client();
    if (!client) {
        // nothing to do
        return 0;
    }
    bool state = string_to_bool(argv[1], client->fullscreen);
    client_set_fullscreen(client, state);
    return 0;
}

