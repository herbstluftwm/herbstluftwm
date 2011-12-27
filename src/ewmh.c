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
static Window      g_wm_window;

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
        { NetCurrentDesktop,        "_NET_CURRENT_DESKTOP"      },
        { NetDesktopNames,          "_NET_DESKTOP_NAMES"        },
        { NetWmDesktop,             "_NET_WM_DESKTOP"           },
        { NetActiveWindow,          "_NET_ACTIVE_WINDOW"        },
        { NetSupportingWmCheck,     "_NET_SUPPORTING_WM_CHECK"  },
        { NetWmName,                "_NET_WM_NAME"              },
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

    /* init for the supporting wm check */
    g_wm_window = XCreateSimpleWindow(g_display, g_root,
                                      42, 42, 42, 42, 0, 0, 0);
    XChangeProperty(g_display, g_root, g_netatom[NetSupportingWmCheck],
        XA_WINDOW, 32, PropModeReplace, (unsigned char*)&(g_wm_window), 1);
    XChangeProperty(g_display, g_wm_window, g_netatom[NetSupportingWmCheck],
        XA_WINDOW, 32, PropModeReplace, (unsigned char*)&(g_wm_window), 1);
    XChangeProperty(g_display, g_wm_window, g_netatom[NetWmName],
        ATOM("UTF8_STRING"), 8, PropModeReplace,
        (unsigned char*)WINDOW_MANAGER_NAME, strlen(WINDOW_MANAGER_NAME)+1);
}

void ewmh_update_all() {
    /* init many properties */
    ewmh_update_client_list();
    ewmh_update_desktops();
    ewmh_update_current_desktop();
    ewmh_update_desktop_names();
}

void ewmh_destroy() {
    g_free(g_windows);
    XDeleteProperty(g_display, g_root, g_netatom[NetSupportingWmCheck]);
    XDestroyWindow(g_display, g_wm_window);
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

void ewmh_update_desktop_names() {
    char**  names = g_new(char*, g_tags->len);
    for (int i = 0; i < g_tags->len; i++) {
        names[i] = g_array_index(g_tags, HSTag*,i)->name->str;
    }
    XTextProperty text_prop;
    Xutf8TextListToTextProperty(g_display, names, g_tags->len,
                                XUTF8StringStyle, &text_prop);
    XSetTextProperty(g_display, g_root, &text_prop, g_netatom[NetDesktopNames]);
}

void ewmh_update_current_desktop() {
    HSTag* tag = get_current_monitor()->tag;
    int index = array_find(g_tags->data, g_tags->len, sizeof(HSTag*), &tag);
    if (index < 0) {
        g_warning("tag %s not found in internal list\n", tag->name->str);
        return;
    }
    XChangeProperty(g_display, g_root, g_netatom[NetCurrentDesktop],
        XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&(index), 1);
}

void ewmh_window_update_tag(Window win, HSTag* tag) {
    int index = array_find(g_tags->data, g_tags->len, sizeof(HSTag*), &tag);
    if (index < 0) {
        g_warning("tag %s not found in internal list\n", tag->name->str);
        return;
    }
    XChangeProperty(g_display, win, g_netatom[NetWmDesktop],
        XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&(index), 1);
}

void ewmh_update_active_window(Window win) {
    XChangeProperty(g_display, g_root, g_netatom[NetActiveWindow],
        XA_WINDOW, 32, PropModeReplace, (unsigned char*)&(win), 1);
}

