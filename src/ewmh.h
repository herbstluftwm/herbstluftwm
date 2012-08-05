/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_EWMH_H_
#define __HERBSTLUFT_EWMH_H_

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xatom.h>
#include <stdbool.h>

#define ENUM_WITH_ALIAS(Identifier, Alias) \
    Identifier, Alias = Identifier

/* actions on NetWmState */
#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

enum {
    NetSupported = 0,
    NetClientList,
    NetClientListStacking,
    NetNumberOfDesktops,
    NetCurrentDesktop,
    NetDesktopNames,
    NetWmDesktop,
    NetDesktopViewport,
    NetActiveWindow,
    NetWmName,
    NetSupportingWmCheck,
    NetWmWindowType,
    NetWmState,
    NetWmWindowOpacity,
    /* window states */
    NetWmStateFullscreen,
    /* window types */
    ENUM_WITH_ALIAS(NetWmWindowTypeDesktop, NetWmWindowTypeFIRST),
    NetWmWindowTypeDock,
    NetWmWindowTypeToolbar,
    NetWmWindowTypeMenu,
    NetWmWindowTypeUtility,
    NetWmWindowTypeSplash,
    NetWmWindowTypeDialog,
    NetWmWindowTypeDropdownMenu,
    NetWmWindowTypePopupMenu,
    NetWmWindowTypeTooltip,
    NetWmWindowTypeNotification,
    NetWmWindowTypeCombo,
    NetWmWindowTypeDnd,
    ENUM_WITH_ALIAS(NetWmWindowTypeNormal, NetWmWindowTypeLAST),
    /* the count of hints */
    NetCOUNT
};

struct HSTag;
struct HSClient;

Atom g_netatom[NetCOUNT];

extern char* g_netatom_names[];

void ewmh_init();
void ewmh_destroy();
void ewmh_update_all();

void ewmh_add_client(Window win);
void ewmh_remove_client(Window win);

void ewmh_update_client_list();
void ewmh_update_client_list_stacking();
void ewmh_update_desktops();
void ewmh_update_desktop_names();
void ewmh_update_active_window(Window win);
void ewmh_update_current_desktop();
void ewmh_update_window_state(struct HSClient* client);
bool ewmh_is_window_state_set(Window win, Atom hint);
bool ewmh_is_fullscreen_set(Window win);

// set the desktop property of a window
void ewmh_window_update_tag(Window win, struct HSTag* tag);

void ewmh_handle_client_message(XEvent* event);

void ewmh_set_window_opacity(Window win, double opacity);

#endif

