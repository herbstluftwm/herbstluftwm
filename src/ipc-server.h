/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_IPC_SERVER_H_
#define __HERBSTLUFT_IPC_SERVER_H_

#include <X11/Xlib.h>
#include <stdbool.h>

void ipc_init();
void ipc_destroy();

void ipc_add_connection(Window window);
// returns true if property was received successfully
bool ipc_handle_connection(Window window);
bool is_ipc_connectable(Window window);

#endif

