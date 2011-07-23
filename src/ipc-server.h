
#ifndef __HERBSTLUFT_IPC_SERVER_H_
#define __HERBSTLUFT_IPC_SERVER_H_

#include <X11/Xlib.h>


void ipc_init();
void ipc_destroy();

void ipc_add_connection(Window window);
void ipc_handle_connection(Window window);
bool is_ipc_connectable(Window window);
void ipc_disconnect_client(Window window);

void ipc_send_success_response(Window window, char* response);

#endif

