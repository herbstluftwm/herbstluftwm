/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "globals.h"
#include "command.h"
#include "utils.h"
#include "ipc-protocol.h"
#include "ipc-server.h"

#include <string.h>
#include <stdio.h>
#include "glib-backports.h"

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <sstream>

// public callable functions
//
void ipc_init() {
}

void ipc_destroy() {
}

void ipc_add_connection(Window window) {
    XSelectInput(g_display, window, PropertyChangeMask);
    // check, if property already exists
    ipc_handle_connection(window);
}

bool ipc_handle_connection(Window win) {
    XTextProperty text_prop;
    if (!XGetTextProperty(g_display, win, &text_prop, ATOM(HERBST_IPC_ARGS_ATOM))) {
        // if the args atom is not present any more then it already has been
        // executed (e.g. after being called by ipc_add_connection())
        return false;
    }
    char** list_return;
    int count;
    if (Success != Xutf8TextPropertyToTextList(g_display, &text_prop, &list_return, &count)) {
        fprintf(stderr, "herbstluftwm: Warning: could not parse the %s atom of herbstclient "
                        "window %d to utf8 list\n",
                        HERBST_IPC_ARGS_ATOM, (unsigned int)win);
        XFree(text_prop.value);
        return false;
    }
    std::ostringstream output;
    int status = call_command(count, list_return, output);
    // send output back
    // Mark this command as executed
    XDeleteProperty(g_display, win, ATOM(HERBST_IPC_ARGS_ATOM));
    XChangeProperty(g_display, win, ATOM(HERBST_IPC_OUTPUT_ATOM),
        ATOM("UTF8_STRING"), 8, PropModeReplace,
        (unsigned char*)output.str().c_str(), 1 + output.str().size());
    // and also set the exit status
    XChangeProperty(g_display, win, ATOM(HERBST_IPC_STATUS_ATOM),
        XA_ATOM, 32, PropModeReplace, (unsigned char*)&(status), 1);
    // cleanup
    XFreeStringList(list_return);
    XFree(text_prop.value);
    return true;
}

bool is_ipc_connectable(Window window) {
    XClassHint hint;
    if (0 == XGetClassHint(g_display, window, &hint)) {
        return false;
    }
    bool is_ipc = false;
    if (hint.res_name && hint.res_class &&
        !strcmp(hint.res_class, HERBST_IPC_CLASS)) {
        is_ipc = true;
    }
    if (hint.res_name) XFree(hint.res_name);
    if (hint.res_class) XFree(hint.res_class);
    return is_ipc;
}

