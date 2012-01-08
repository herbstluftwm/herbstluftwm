/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "ewmh.h"
#include "utils.h"
#include "globals.h"
#include "layout.h"
#include "clientlist.h"
#include "settings.h"

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
int*        g_focus_stealing_prevention;

/* list of names of all _NET-atoms */
char* g_netatom_names[NetCOUNT] = {
    [ NetSupported                  ] = "_NET_SUPPORTED"                    ,
    [ NetClientList                 ] = "_NET_CLIENT_LIST"                  ,
    [ NetClientListStacking         ] = "_NET_CLIENT_LIST_STACKING"         ,
    [ NetNumberOfDesktops           ] = "_NET_NUMBER_OF_DESKTOPS"           ,
    [ NetCurrentDesktop             ] = "_NET_CURRENT_DESKTOP"              ,
    [ NetDesktopNames               ] = "_NET_DESKTOP_NAMES"                ,
    [ NetWmDesktop                  ] = "_NET_WM_DESKTOP"                   ,
    [ NetDesktopViewport            ] = "_NET_DESKTOP_VIEWPORT"             ,
    [ NetWorkarea                   ] = "_NET_WORKAREA"                     ,
    [ NetActiveWindow               ] = "_NET_ACTIVE_WINDOW"                ,
    [ NetWmName                     ] = "_NET_WM_NAME"                      ,
    [ NetWmWindowType               ] = "_NET_WM_WINDOW_TYPE"               ,
    [ NetWmState                    ] = "_NET_WM_STATE"                     ,
    [ NetWmStateFullscreen          ] = "_NET_WM_STATE_FULLSCREEN"          ,
    [ NetSupportingWmCheck          ] = "_NET_SUPPORTING_WM_CHECK"          ,
    [ NetWmWindowTypeDesktop        ] = "_NET_WM_WINDOW_TYPE_DESKTOP"       ,
    [ NetWmWindowTypeDock           ] = "_NET_WM_WINDOW_TYPE_DOCK"          ,
    [ NetWmWindowTypeToolbar        ] = "_NET_WM_WINDOW_TYPE_TOOLBAR"       ,
    [ NetWmWindowTypeMenu           ] = "_NET_WM_WINDOW_TYPE_MENU"          ,
    [ NetWmWindowTypeUtility        ] = "_NET_WM_WINDOW_TYPE_UTILITY"       ,
    [ NetWmWindowTypeSplash         ] = "_NET_WM_WINDOW_TYPE_SPLASH"        ,
    [ NetWmWindowTypeDialog         ] = "_NET_WM_WINDOW_TYPE_DIALOG"        ,
    [ NetWmWindowTypeDropdownMenu   ] = "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU" ,
    [ NetWmWindowTypePopupMenu      ] = "_NET_WM_WINDOW_TYPE_POPUP_MENU"    ,
    [ NetWmWindowTypeTooltip        ] = "_NET_WM_WINDOW_TYPE_TOOLTIP"       ,
    [ NetWmWindowTypeNotification   ] = "_NET_WM_WINDOW_TYPE_NOTIFICATION"  ,
    [ NetWmWindowTypeCombo          ] = "_NET_WM_WINDOW_TYPE_COMBO"         ,
    [ NetWmWindowTypeDnd            ] = "_NET_WM_WINDOW_TYPE_DND"           ,
    [ NetWmWindowTypeNormal         ] = "_NET_WM_WINDOW_TYPE_NORMAL"        ,
};

void ewmh_init() {
    /* init globals */
    g_focus_stealing_prevention =
        &(settings_find("focus_stealing_prevention")->value.i);

    /* init ewmh net atoms */
    for (int i = 0; i < NetCOUNT; i++) {
        if (g_netatom_names[i] == NULL) {
            g_warning("no name specified in g_netatom_names "
                      "for atom number %d\n", i);
            continue;
        }
        g_netatom[i] = XInternAtom(g_display, g_netatom_names[i], False);
    }

    /* tell which ewmh atoms are supported */
    XChangeProperty(g_display, g_root, g_netatom[NetSupported], XA_ATOM, 32,
        PropModeReplace, (unsigned char *) g_netatom, NetCOUNT);

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

    /* init atoms that never change */
    int buf[] = { 0, 0 };
    XChangeProperty(g_display, g_root, g_netatom[NetDesktopViewport],
        XA_CARDINAL, 32, PropModeReplace, (unsigned char *) buf, LENGTH(buf));
}

void ewmh_update_all() {
    /* init many properties */
    ewmh_update_client_list();
    ewmh_update_desktops();
    ewmh_update_current_desktop();
    ewmh_update_desktop_names();
    ewmh_update_workarea();
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

void ewmh_update_workarea() {
    if (g_monitors->len == 0) {
        // a workaround so that the NetWorkarea only is updated if the monitors
        // aren't set up yet.
        // TODO: find a cleaner solution for this, e.g. a ewmh-update-queue
        return;
    }
    XRectangle rect = monitor_get_workarea(get_current_monitor());

    // init the buf for the desktop rectangles
    size_t len = 4 * g_tags->len;
    long* areas = g_new(long, len);
    for (int i = 0; i < g_tags->len; i++) {
        areas[4 * i + 0] = rect.x;
        areas[4 * i + 1] = rect.y;
        areas[4 * i + 2] = rect.width;
        areas[4 * i + 3] = rect.height;
    }
    XChangeProperty(g_display, g_root, g_netatom[NetWorkarea],
        XA_CARDINAL, 32, PropModeReplace, (unsigned char *) areas, len);
    g_free(areas);
}

void ewmh_update_desktops() {
    XChangeProperty(g_display, g_root, g_netatom[NetNumberOfDesktops],
        XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&(g_tags->len), 1);
    ewmh_update_workarea();
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
    g_free(names);
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

bool focus_stealing_allowed(long source) {
    if (*g_focus_stealing_prevention) {
        /* only allow it to pagers/taskbars */
        return (source == 2);
    } else {
        /* no prevention */
        return true;
    }
}

void ewmh_handle_client_message(XEvent* event) {
    HSDebug("Received event: ClientMessage\n");
    XClientMessageEvent* me = &(event->xclient);
    int index;
    for (index = 0; index < NetCOUNT; index++) {
        if (me->message_type == g_netatom[index]) {
            break;
        }
    }
    if (index >= NetCOUNT) {
        HSDebug("recieved unknown client message\n");
        return;
    }
    HSClient* client;

    int desktop_index;
    switch (index) {
        case NetActiveWindow:
            // only steal focus it allowed to the current source
            // (i.e.  me->data.l[0] in this case as specified by EWMH)
            if (focus_stealing_allowed(me->data.l[0])) {
                focus_window(me->window, true, true);
            }
            break;

        case NetCurrentDesktop:
            desktop_index = me->data.l[0];
            if (desktop_index < 0 || desktop_index >= g_tags->len) {
                HSDebug("_NET_CURRENT_DESKTOP: invalid index \"%d\"\n",
                        desktop_index);
                break;
            }
            HSTag* tag = g_array_index(g_tags, HSTag*, desktop_index);
            monitor_set_tag(get_current_monitor(), tag);
            break;

        case NetWmState:
            client = get_client_from_window(me->window);
            /* ignore requests for unmanaged windows */
            if (!client) break;

            /* mapping between EWMH atoms and client struct members */
            struct {
                int     atom_index;
                bool    enabled;
                void    (*callback)(HSClient*, bool);
            } client_atoms[] = {
                { NetWmStateFullscreen,
                    client->fullscreen,     client_set_fullscreen },
            };

            /* me->data.l[1] and [2] describe the properties to alter */
            for (int prop = 1; prop <= 2; prop++) {
                if (me->data.l[prop] == 0) {
                    /* skip if no property is specified */
                    continue;
                }
                /* check if we support the property data[prop] */
                int i;
                for (i = 0; i < LENGTH(client_atoms); i++) {
                    if (g_netatom[client_atoms[i].atom_index]
                        == me->data.l[prop]) {
                        break;
                    }
                }
                if (i >= LENGTH(client_atoms)) {
                    /* property will not be handled */
                    continue;
                }
                bool new_value[] = {
                    [ _NET_WM_STATE_REMOVE  ] = false,
                    [ _NET_WM_STATE_ADD     ] = true,
                    [ _NET_WM_STATE_TOGGLE  ] = !client_atoms[i].enabled,
                };
                int action = me->data.l[0];
                if (action >= LENGTH(new_value)) {
                    HSDebug("_NET_WM_STATE: invalid action %d\n", action);
                }
                /* change the value */
                client_atoms[i].callback(client, new_value[action]);
            }
            break;

        default:
            HSDebug("no handler for the client message \"%s\"\n",
                    g_netatom_names[index]);
            break;
    }
}

void ewmh_update_window_state(struct HSClient* client) {
    /* mapping between EWMH atoms and client struct members */
    struct {
        int     atom_index;
        bool    enabled;
    } client_atoms[] = {
        { NetWmStateFullscreen,         client->fullscreen      },
    };

    /* find out which flags are set */
    Atom window_state[LENGTH(client_atoms)];
    size_t count_enabled = 0;
    for (int i = 0; i < LENGTH(client_atoms); i++) {
        if (client_atoms[i].enabled) {
            window_state[count_enabled] = g_netatom[client_atoms[i].atom_index];
            count_enabled++;
        }
    }

    /* write it to the window */
    XChangeProperty(g_display, client->window, g_netatom[NetWmState], XA_ATOM,
        32, PropModeReplace, (unsigned char *) window_state, count_enabled);
}

