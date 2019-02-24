#ifndef __HERBSTLUFT_IPC_SERVER_H_
#define __HERBSTLUFT_IPC_SERVER_H_

#include <X11/X.h>

void ipc_add_connection(Window win);
// returns true if property was received successfully
bool ipc_handle_connection(Window window);
bool is_ipc_connectable(Window window);

#endif

