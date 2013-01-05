/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include <X11/Xlib.h>
#include <glib.h>

typedef struct HCConnection HCConnection;

HCConnection* hc_connect();
HCConnection* hc_connect_to_display(Display* display);
void hc_disconnect(HCConnection* con);
/* ensure there is a client window for sending commands */
bool hc_create_client_window(HCConnection* con);

bool hc_send_command(HCConnection* con, int argc, char* argv[],
                     GString** ret_out, int* ret_status);
bool hc_send_command_once(int argc, char* argv[],
                          GString** ret_out, int* ret_status);

bool hc_hook_window_connect(HCConnection* con);
bool hc_next_hook(HCConnection* con, int* argc, char** argv[]);

