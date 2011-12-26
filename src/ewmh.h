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
    NetLast
};

Atom g_netatom[NetLast];

void ewmh_init();
void ewmh_destroy();

void ewmh_add_client(Window win);
void ewmh_remove_client(Window win);

void ewmh_update_client_list();

#endif

