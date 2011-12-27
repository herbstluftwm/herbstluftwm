/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_EWMH_H_
#define __HERBSTLUFT_EWMH_H_

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xatom.h>

enum {
    NetSupported,
    NetClientList,
    NetClientListStacking,
    NetNumberOfDesktops,
    NetDesktopNames,
    NetCurrentDesktop,
    NetWmDesktop,
    NetActiveWindow,
    NetLast
};

struct HSTag;

Atom g_netatom[NetLast];

void ewmh_init();
void ewmh_destroy();
void ewmh_update_all();

void ewmh_add_client(Window win);
void ewmh_remove_client(Window win);

void ewmh_update_client_list();
void ewmh_update_desktops();
void ewmh_update_desktop_names();
void ewmh_update_active_window(Window win);
void ewmh_update_current_desktop();

// set the desktop property of a window
void ewmh_window_update_tag(Window win, struct HSTag* tag);

#endif

