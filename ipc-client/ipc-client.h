/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include <X11/Xlib.h>
#include <stdbool.h>

#ifndef __HERBSTLUFT_IPC_CLIENT_H_
#define __HERBSTLUFT_IPC_CLIENT_H_

typedef struct HCConnection HCConnection;

/** Connect to hlwm via an X11 display. This does not check whether
 * herbstluftwm is (still) running. Use hc_check_running() for this
 */
HCConnection* hc_connect();
HCConnection* hc_connect_to_display(Display* display);
/** check whether herbstluftwm is running */
bool hc_check_running(HCConnection* con);
void hc_disconnect(HCConnection* con);

int hc_connection_socket(HCConnection* con);
void hc_process_events(HCConnection* con);

/* ensure there is a client window for sending commands */
bool hc_create_client_window(HCConnection* con);

/**
 * @brief send a command and wait for its reply to arrive
 * @param the connection to hlwm
 * @param number of arguments of the ipc call
 * @param the arguments of the ipc call
 * @param the returned output channel (needs to be free'd by the caller on success)
 * @param the returned error channel (needs to be free'd by the caller on success)
 * @param the returned exit status
 * @return whether the reply was received successfully.
 * if false is returned, then none of the ret_...-pointers is set.
 */
bool hc_send_command(HCConnection* con, int argc, char* argv[],
                     char** ret_out, char** ret_err, int* ret_status);

bool hc_hook_window_connect(HCConnection* con);
bool hc_next_hook(HCConnection* con, int* argc, char** argv[]);

#endif

