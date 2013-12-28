/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "ewmh.h"
#include "utils.h"
#include "globals.h"
#include "layout.h"
#include "clientlist.h"
#include "settings.h"
#include "stack.h"
#include "mouse.h"

#include "glib-backports.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

Window*     g_windows; // array with Window-IDs
size_t      g_window_count;
static Window      g_wm_window;
int*        g_focus_stealing_prevention;

static Window*  g_original_clients = NULL;
static unsigned long g_original_clients_count = 0;
static bool ewmh_read_client_list(Window** buf, unsigned long *count);

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
    [ NetActiveWindow               ] = "_NET_ACTIVE_WINDOW"                ,
    [ NetWmName                     ] = "_NET_WM_NAME"                      ,
    [ NetWmWindowType               ] = "_NET_WM_WINDOW_TYPE"               ,
    [ NetWmState                    ] = "_NET_WM_STATE"                     ,
    [ NetWmWindowOpacity            ] = "_NET_WM_WINDOW_OPACITY"            ,
    [ NetMoveresizeWindow           ] = "_NET_MOVERESIZE_WINDOW"            ,
    [ NetWmMoveresize               ] = "_NET_WM_MOVERESIZE"                ,
    [ NetWmStateFullscreen          ] = "_NET_WM_STATE_FULLSCREEN"          ,
    [ NetWmStateDemandsAttention    ] = "_NET_WM_STATE_DEMANDS_ATTENTION"   ,
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
    if (!ewmh_read_client_list(&g_original_clients, &g_original_clients_count))
    {
        g_original_clients = NULL;
        g_original_clients_count = 0;
    }

    /* init for the supporting wm check */
    g_wm_window = XCreateSimpleWindow(g_display, g_root,
                                      42, 42, 42, 42, 0, 0, 0);
    XChangeProperty(g_display, g_root, g_netatom[NetSupportingWmCheck],
        XA_WINDOW, 32, PropModeReplace, (unsigned char*)&(g_wm_window), 1);
    XChangeProperty(g_display, g_wm_window, g_netatom[NetSupportingWmCheck],
        XA_WINDOW, 32, PropModeReplace, (unsigned char*)&(g_wm_window), 1);
    ewmh_update_wmname();

    /* init atoms that never change */
    int buf[] = { 0, 0 };
    XChangeProperty(g_display, g_root, g_netatom[NetDesktopViewport],
        XA_CARDINAL, 32, PropModeReplace, (unsigned char *) buf, LENGTH(buf));
}

void ewmh_update_all() {
    /* init many properties */
    ewmh_update_client_list();
    ewmh_update_client_list_stacking();
    ewmh_update_desktops();
    ewmh_update_current_desktop();
    ewmh_update_desktop_names();
}

void ewmh_destroy() {
    g_free(g_windows);
    if (g_original_clients) {
        XFree(g_original_clients);
    }
    XDeleteProperty(g_display, g_root, g_netatom[NetSupportingWmCheck]);
    XDestroyWindow(g_display, g_wm_window);
}

void ewmh_set_wmname(char* name) {
    XChangeProperty(g_display, g_wm_window, g_netatom[NetWmName],
        ATOM("UTF8_STRING"), 8, PropModeReplace,
        (unsigned char*)name, strlen(name));
    XChangeProperty(g_display, g_root, g_netatom[NetWmName],
        ATOM("UTF8_STRING"), 8, PropModeReplace,
        (unsigned char*)name, strlen(name));
}

void ewmh_update_wmname() {
    ewmh_set_wmname(settings_find("wmname")->value.s);
}

void ewmh_update_client_list() {
    XChangeProperty(g_display, g_root, g_netatom[NetClientList],
        XA_WINDOW, 32, PropModeReplace,
        (unsigned char *) g_windows, g_window_count);
}

static bool ewmh_read_client_list(Window** buf, unsigned long *count) {
    Atom actual_type;
    int format;
    unsigned long bytes_left;
    if (Success != XGetWindowProperty(g_display, g_root,
            g_netatom[NetClientList], 0, ~0L, False, XA_WINDOW, &actual_type,
            &format, count, &bytes_left, (unsigned char**)buf)) {
        return false;
    }
    if (bytes_left || actual_type != XA_WINDOW || format != 32) {
        return false;
    }
    return true;
}

void ewmh_get_original_client_list(Window** buf, unsigned long *count) {
    *buf = g_original_clients;
    *count = g_original_clients_count;
}

struct ewmhstack {
    Window* buf;
    int     count;
    int     i;  // index of the next free element in buf
};

static void ewmh_add_tag_stack(HSTag* tag, void* data) {
    struct ewmhstack* stack = (struct ewmhstack*)data;
    if (find_monitor_with_tag(tag)) {
        // do not add tags because they are already added
        return;
    }
    int remain;
    stack_to_window_buf(tag->stack, stack->buf + stack->i,
                        stack->count - stack->i, true, &remain);
    if (remain >= 0) {
        stack->i = stack->count - remain;
    } else {
        HSDebug("Warning: not enough space in the ewmh stack\n");
        stack->i = stack->count;
    }
}

void ewmh_update_client_list_stacking() {
    // First: get the windows in the current stack
    struct ewmhstack stack;
    stack.count = g_window_count;
    stack.buf = g_new(Window, stack.count);
    int remain;
    monitor_stack_to_window_buf(stack.buf, stack.count, true, &remain);
    stack.i = stack.count - remain;

    // Then add all the others at the end
    tag_foreach(ewmh_add_tag_stack, &stack);

    // reverse stacking order, because ewmh requires bottom to top order
    array_reverse(stack.buf, stack.count, sizeof(stack.buf[0]));

    XChangeProperty(g_display, g_root, g_netatom[NetClientListStacking],
        XA_WINDOW, 32, PropModeReplace,
        (unsigned char *) stack.buf, stack.i);
    g_free(stack.buf);
}

void ewmh_add_client(Window win) {
    g_windows = g_renew(Window, g_windows, g_window_count + 1);
    g_windows[g_window_count] = win;
    g_window_count++;
    ewmh_update_client_list();
    ewmh_update_client_list_stacking();
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
    ewmh_update_client_list_stacking();
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
    XFree(text_prop.value);
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

static bool focus_stealing_allowed(long source) {
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
        HSDebug("received unknown client message\n");
        return;
    }
    HSClient* client;

    int desktop_index;
    switch (index) {
        case NetActiveWindow:
            // only steal focus it allowed to the current source
            // (i.e.  me->data.l[0] in this case as specified by EWMH)
            if (focus_stealing_allowed(me->data.l[0])) {
                HSClient* client = get_client_from_window(me->window);
                if (client) {
                    focus_client(client, true, true);
                }
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

        case NetWmDesktop:
            desktop_index = me->data.l[0];
            if (!focus_stealing_allowed(me->data.l[1])) {
                break;
            }
            HSTag* target = get_tag_by_index(desktop_index);
            client = get_client_from_window(me->window);
            if (client && target) {
                tag_move_client(client, target);
            }
            break;

        case NetWmState:
            client = get_client_from_window(me->window);
            /* ignore requests for unmanaged windows */
            if (!client || !client->ewmhrequests) break;

            /* mapping between EWMH atoms and client struct members */
            struct {
                int     atom_index;
                bool    enabled;
                void    (*callback)(HSClient*, bool);
            } client_atoms[] = {
                { NetWmStateFullscreen,
                    client->fullscreen,     client_set_fullscreen },
                { NetWmStateDemandsAttention,
                    client->urgent,         client_set_urgent },
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

        case NetWmMoveresize:
            // TODO: handle requests more exactly
            mouse_start_drag(me->window, mouse_function_resize);
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
        { NetWmStateFullscreen,         client->ewmhfullscreen  },
        { NetWmStateDemandsAttention,   client->urgent          },
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

bool ewmh_is_window_state_set(Window win, Atom hint) {
    Atom* states;
    Atom actual_type;
    int format;
    unsigned long actual_count, bytes_left;
    if (Success != XGetWindowProperty(g_display, win, g_netatom[NetWmState], 0,
            ~0L, False, XA_ATOM, &actual_type, &format, &actual_count,
            &bytes_left, (unsigned char**)&states)) {
        // NetWmState just is not set properly
        return false;
    }
    if (actual_type != XA_ATOM || format != 32 || states == NULL) {
        // invalid format or no entries
        return false;
    }
    bool hint_set = false;
    for (int i = 0; i < actual_count; i++) {
        if (states[i] == hint) {
            hint_set = true;
            break;
        }
    }
    XFree(states);
    return hint_set;
}

bool ewmh_is_fullscreen_set(Window win) {
    return ewmh_is_window_state_set(win, g_netatom[NetWmStateFullscreen]);
}

void ewmh_set_window_opacity(Window win, double opacity) {
    uint32_t int_opacity = UINT32_MAX * CLAMP(opacity, 0, 1);

    XChangeProperty(g_display, win, g_netatom[NetWmWindowOpacity], XA_CARDINAL,
                    32, PropModeReplace, (unsigned char*)&int_opacity, 1);
}

