
#include "globals.h"
#include "utils.h"
#include "ipc-protocol.h"
#include "ipc-server.h"

#include <string.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <glib.h>

typedef struct ClientConnection {
    Window      window; // window to talk with
    int         argc;   // total number of args, -1 if unknown;
    GString**   argv;   // all the args, but NULL if argc is unknown
    int         cur_arg;// index of the arg for the next received message
                        // its value is undefined if argc is unknown
} ClientConnection;

ClientConnection g_cur_connection;
GHashTable* g_connections;

// some helperfunctions for the g_connections-GHashTable
//
guint hash_window_id(const Window* key) {
    return (guint) *key;
}

gboolean window_id_equals(Window* a, Window* b) {
    return *a == *b;
}

void execute_ipc_call(ClientConnection* connection) {
    printf("now i am executing for %d\n", (int)(connection->window));
    printf("there are %d args:\n", connection->argc);
    int i;
    for (i = 0; i < connection->argc; i++) {
        printf("   %2d => \"%s\"\n", i, connection->argv[i]->str);
    }
}


// free ClientConnection in a GHashTable
void free_entry(Window* key, ClientConnection* connection, gpointer data) {
    (void) key;
    (void) data;
    ipc_send_success_response(connection->window, "IPC-Server Shutdown");
    if (connection->argv) {
        g_free(connection->argv);
    }
    g_free(connection);
}

// public callable functions
//
void ipc_init() {
    g_connections = g_hash_table_new((GHashFunc)hash_window_id,
                                     (GEqualFunc)window_id_equals);
}

void ipc_destroy() {
    // TODO: is it really ok to first free the keys and values before destroy()?
    g_hash_table_foreach(g_connections, (GHFunc)free_entry, 0);
    g_hash_table_destroy(g_connections);
}

void ipc_add_connection(Window window) {
    ipc_send_success_response(window, HERBST_IPC_SUCCESS);
    // create new client connection
    ClientConnection* new_connection = g_new(ClientConnection, 1);
    new_connection->window = window;
    new_connection->argc = -1;
    new_connection->argv = NULL;
    // insert in the connection-pool
    g_hash_table_insert(g_connections, &(new_connection->window), new_connection);
    // listen for propertychange-events on this window
    XSelectInput(g_display, window, PropertyChangeMask);
}


void ipc_handle_connection(Window window) {
    // check if this window already is known
    ClientConnection* connection = g_hash_table_lookup(g_connections, &window);
    if (connection == NULL) {
        ipc_add_connection(window);
    } else {
        // find out next step
        if (connection->argc < 0) {
            // wait for transmission of argc
            int *value;
            Atom type;
            int format;
            unsigned long items, bytes;
            int status = XGetWindowProperty(g_display, window,
                ATOM(HERBST_IPC_ARGC_ATOM), 0, 1, False,
                XA_ATOM, &type, &format, &items, &bytes, (unsigned char**)&value);
            if (status != Success) {
                ipc_send_success_response(window, "Wrong ARGC received");
            } else {
                connection->argc = *value;
                XFree(value);
                ipc_send_success_response(window, HERBST_IPC_SUCCESS);
                // now argc is known => create argv
                connection->argv = g_new(GString*, connection->argc);
                // start filling of argv
                connection->cur_arg = 0;
            }
        } else if (connection->cur_arg < connection->argc) {
            // if there are still some args to be parsed
            // then read next arg from atom
            GString* result = g_string_new("");
            long bufsize = 10;
            char *buf;
            Atom type;
            int format;
            unsigned long items, bytes;
            long offset = 0;
            bool parse_error_occured = false;
            do {
                int status = XGetWindowProperty(g_display, window,
                    ATOM(HERBST_IPC_ARGV_ATOM), offset, bufsize, False,
                    ATOM("UTF8_STRING"), &type, &format,
                    &items, &bytes, (unsigned char**)&buf);
                if (status != Success) {
                    ipc_send_success_response(window, "Wrong ARGV received");
                    parse_error_occured = true;
                    break; // then stop parsing
                } else {
                    result = g_string_append(result, buf);
                    offset += bufsize;
                    XFree(buf);
                }
                //printf("recieved: \"%s\"\n", result->str);
            } while (bytes > 0);
            // write result
            if (!parse_error_occured) {
                connection->argv[connection->cur_arg] = result;
                connection->cur_arg++;
                ipc_send_success_response(window, HERBST_IPC_SUCCESS);
            }
        }
        // check, if function can be executed
        if (connection->cur_arg >= connection->argc) {
            // if enough args are parsed, then execute it!
            execute_ipc_call(connection);
        }
    }
}

bool is_ipc_connectable(Window window) {
    XClassHint hint;
    XGetClassHint(g_display, window, &hint);
    bool is_ipc = false;
    if (hint.res_name && hint.res_class &&
        !strcmp(hint.res_class, HERBST_IPC_CLASS)) {
        is_ipc = true;
    }
    XFree(hint.res_name);
    XFree(hint.res_class);
    return is_ipc;
}

void ipc_disconnect_client(Window window) {
    ClientConnection* connection = g_hash_table_lookup(g_connections, &window);
    if (connection != NULL) {
        if (connection->argv) {
            g_free(connection->argv);
        }
        g_free(connection);
    }
    g_hash_table_remove(g_connections, &window);
}

void ipc_send_success_response(Window window, char* response) {
    XEvent msg;
    msg.type = ClientMessage;
    msg.xany.display = g_display;
    msg.xany.window = window;
    msg.xclient.format = 8;
    // maximum datasize in XClientMessageEvent is 20 bytes
    strncpy(msg.xclient.data.b, response, 20-1);
    msg.xclient.data.b[20-1] = '\0';
    XSendEvent(g_display, window, False, 0, &msg);
    XFlush(g_display);
}

