/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ipc-protocol.h"
#include "client-utils.h"
#include "ipc-client.h"

struct HCConnection {
    Display*    display;
    bool        own_display; // if we have to close it on disconnect
    Window      hook_window;
    Window      client_window;
    Atom        atom_args;
    Atom        atom_output;
    Atom        atom_status;
    Window      root;
};

HCConnection* hc_connect() {
    Display* display = XOpenDisplay(NULL);
    if (display == NULL) {
        return NULL;
    }
    HCConnection* con = hc_connect_to_display(display);
    if (con) {
        con->own_display = true;
    }
    return con;
}

HCConnection* hc_connect_to_display(Display* display) {
    HCConnection* con = malloc(sizeof(struct HCConnection));
    if (!con) {
        return con;
    }
    memset(con, 0, sizeof(HCConnection));
    con->display = display;
    con->root = DefaultRootWindow(con->display);
    con->atom_args = XInternAtom(con->display, HERBST_IPC_ARGS_ATOM, False);
    con->atom_output = XInternAtom(con->display, HERBST_IPC_OUTPUT_ATOM, False);
    con->atom_status = XInternAtom(con->display, HERBST_IPC_STATUS_ATOM, False);
    return con;
}

void hc_disconnect(HCConnection* con) {
    if (!con) {
        return;
    }
    if (con->client_window) {
        XDestroyWindow(con->display, con->client_window);
    }
    if (con->own_display) {
        XCloseDisplay(con->display);
    }
    free(con);
}

bool hc_create_client_window(HCConnection* con) {
    if (con->client_window) {
        return true;
    }
    /* ensure that classhint and the command is set when the hlwm-server
     * receives the XCreateWindowEvent */
    XGrabServer(con->display);
    // create window
    con->client_window = XCreateSimpleWindow(con->display, con->root,
                                             42, 42, 42, 42, 0, 0, 0);
    // set wm_class for window
    XClassHint *hint = XAllocClassHint();
    hint->res_name = HERBST_IPC_CLASS;
    hint->res_class = HERBST_IPC_CLASS;
    XSetClassHint(con->display, con->client_window, hint);
    XFree(hint);
    XSelectInput(con->display, con->client_window, PropertyChangeMask);
    /* the window has been initialized properly, now allow the server to
     * receive the event for it */
    XUngrabServer(con->display);
    return true;
}

bool hc_send_command(HCConnection* con, int argc, char* argv[],
                     char** ret_out, int* ret_status) {
    if (!hc_create_client_window(con)) {
        return false;
    }
    // set arguments
    XTextProperty text_prop;
    Xutf8TextListToTextProperty(con->display, argv, argc, XUTF8StringStyle, &text_prop);
    XSetTextProperty(con->display, con->client_window, &text_prop, con->atom_args);
    XFree(text_prop.value);

    // get output
    int command_status = 0;
    XEvent event;
    char* output = NULL;
    bool output_received = false, status_received = false;
    while (!output_received || !status_received) {
        XNextEvent(con->display, &event);
        if (event.type != PropertyNotify) {
            // got an event of wrong type
            continue;
        }
        XPropertyEvent* pe = &(event.xproperty);
        if (pe->window != con->client_window) {
            // got an event from wrong window
            continue;
        }
        if (!output_received
            && pe->atom == con->atom_output) {
            output = read_window_property(con->display, con->client_window,
                                          con->atom_output);
            if (!output) {
                fprintf(stderr, "could not get WindowProperty \"%s\"\n",
                                HERBST_IPC_OUTPUT_ATOM);
                return false;
            }
            output_received = true;
        }
        else if (!status_received && pe->atom == con->atom_status) {
            long* value;
            Atom type;
            int format;
            unsigned long items, bytes;
            if (Success != XGetWindowProperty(con->display, con->client_window,
                    XInternAtom(con->display, HERBST_IPC_STATUS_ATOM, False), 0, 1, False,
                    XA_ATOM, &type, &format, &items, &bytes, (unsigned char**)&value)
                || format != 32) {
                    // if could not get window property
                fprintf(stderr, "could not get WindowProperty \"%s\"\n",
                                HERBST_IPC_STATUS_ATOM);
                return false;
            }
            command_status = *value;
            XFree(value);
            status_received = true;
        }
    }
    *ret_status = command_status;
    *ret_out = output;
    return true;
}

static bool g_bad_window_occurred = false;

static int log_bad_window_error(Display* display, XErrorEvent* ev) {
    g_bad_window_occurred = true;
    return -1;
}

static Window get_hook_window(Display* display) {
    long* value;
    Atom type;
    int format;
    unsigned long items, bytes;
    int status = XGetWindowProperty(display, DefaultRootWindow(display),
        XInternAtom(display, HERBST_HOOK_WIN_ID_ATOM, False), 0, 1, False,
        XA_ATOM, &type, &format, &items, &bytes, (unsigned char**)&value);
    // only accept exactly one Window id
    if (status != Success || items != 1 || format != 32) {
        return 0;
    }
    Window win = *value;
    XFree(value);
    // check that the window 'win' still exists
    // set our custom error handler and back up old handler
    int (*old_error_handler)(Display *, XErrorEvent *) =
        XSetErrorHandler(log_bad_window_error);
    XWindowAttributes attr;
    // check whether the window still exists. If it does not exist,
    // log_bad_window_error() is called.
    g_bad_window_occurred = false;
    XGetWindowAttributes(display, win, &attr);
    XSync(display, False);
    // restore old handler
    XSetErrorHandler(old_error_handler);
    // if the handler was called, then the window does not exist anymore
    if (g_bad_window_occurred) {
        return 0;
    }
    return win;
}

bool hc_check_running(HCConnection* con) {
    return get_hook_window(con->display) != 0;
}

bool hc_hook_window_connect(HCConnection* con) {
    if (con->hook_window) {
        return true;
    }
    con->hook_window = get_hook_window(con->display);
    if (!con->hook_window) {
        return false;
    }
    long mask = StructureNotifyMask|PropertyChangeMask;
    XSelectInput(con->display, con->hook_window, mask);
    return true;
}

bool hc_next_hook(HCConnection* con, int* argc, char** argv[]) {
    if (!hc_hook_window_connect(con)) {
        return false;
    }
    // get window to listen at
    Window win = con->hook_window;
    // listen on window
    XEvent next_event;
    bool received_hook = false;
    while (!received_hook) {
        XNextEvent(con->display, &next_event);
        if (next_event.type == DestroyNotify) {
            if (next_event.xdestroywindow.window == win) {
                // hook window was destroyed
                // so quit idling
                return false;
            }
        }
        if (next_event.type != PropertyNotify) {
            fprintf(stderr, "Warning: got other event than PropertyNotify\n");
            continue;
        }
        XPropertyEvent* pe = &next_event.xproperty;
        if (pe->state == PropertyDelete) {
            // just ignore property delete events
            continue;
        }
        if (pe->window != win) {
            fprintf(stderr, "Warning: expected event from window %u", (unsigned int)win);
            fprintf(stderr, " but got something from %u\n", (unsigned int)pe->window);
            continue;
        }
        XTextProperty text_prop;
        XGetTextProperty(con->display, win, &text_prop, pe->atom);
        char** list_return;
        int count;
        if (Success != Xutf8TextPropertyToTextList(con->display, &text_prop,
                                                   &list_return, &count)) {
            XFree(text_prop.value);
            return false;
        }
        *argc = count;
        *argv = argv_duplicate(count, list_return); // has to be freed by caller
        received_hook = true;
        // cleanup
        XFreeStringList(list_return);
        XFree(text_prop.value);
    }
    return true;
}
