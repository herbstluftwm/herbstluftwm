/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "ewmh.h"
#include "utils.h"
#include "globals.h"
#include "layout.h"

#include <glib.h>
#include <string.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

Window*     g_windows; // array with Window-IDs
size_t      g_window_count;

void ewmh_init() {
    /* init ewmh net atoms */
    struct {
        int     atom;
        char*   name;
    } a2n[] = {
        { NetSupported,             "_NET_SUPPORTED"            },
        { NetClientList,            "_NET_CLIENT_LIST"          },
        { NetClientListStacking,    "_NET_CLIENT_LIST_STACKING" },
        { NetNumberOfDesktops,      "_NET_NUMBER_OF_DESKTOPS"   },
    };
    for (int i = 0; i < LENGTH(a2n); i++) {
        g_netatom[a2n[i].atom] = XInternAtom(g_display, a2n[i].name, False);
    }
    /* tell which ewmh atoms are supported */
    XChangeProperty(g_display, g_root, g_netatom[NetSupported], XA_ATOM, 32,
        PropModeReplace, (unsigned char *) g_netatom, NetLast);

    /* init some globals */
    g_windows = NULL;
    g_window_count = 0;

    /* init many properties */
    ewmh_update_client_list();
    ewmh_update_desktops();
}

void ewmh_destroy() {
    g_free(g_windows);
}

void ewmh_update_client_list() {
    XChangeProperty(g_display, g_root, g_netatom[NetClientList],
        XA_WINDOW, 32, PropModeReplace,
        (unsigned char *) g_windows, g_window_count);
    XChangeProperty(g_display, g_root, g_netatom[NetClientListStacking],
        XA_WINDOW, 32, PropModeReplace,
        (unsigned char *) g_windows, g_window_count);
}

void ewmh_add_client(Window win) {
    g_windows = g_renew(Window, g_windows, g_window_count + 1);
    g_windows[g_window_count] = win;
    g_window_count++;
    ewmh_update_client_list();
}

void ewmh_remove_client(Window win) {
    int index = array_find(g_windows, g_window_count,
                           sizeof(Window), &win);
    if (index < 0) {
        g_warning("could not find window %lx in g_windows\n", win);
    } else {
        g_memmove(g_windows + index, g_windows + index + 1,
                  sizeof(Window) *(g_window_count - index - 1));
        g_windows = g_renew(Window, g_windows, g_window_count - 1);
        g_window_count--;
    }
    ewmh_update_client_list();
}

void ewmh_update_desktops() {
    XChangeProperty(g_display, g_root, g_netatom[NetNumberOfDesktops],
        XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&(g_tags->len), 1);
}

