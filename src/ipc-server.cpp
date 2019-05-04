#include "ipc-server.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdio>
#include <sstream>

#include "command.h"
#include "ipc-protocol.h"
#include "xconnection.h"

IpcServer::IpcServer(XConnection& xconnection)
    : X(xconnection) {
}

void IpcServer::addConnection(Window window) {
    XSelectInput(X.display(), window, PropertyChangeMask);
    // check, if property already exists
    handleConnection(window);
}

bool IpcServer::handleConnection(Window win) {
    XTextProperty text_prop;
    if (!XGetTextProperty(X.display(), win, &text_prop, X.atom(HERBST_IPC_ARGS_ATOM))) {
        // if the args atom is not present any more then it already has been
        // executed (e.g. after being called by ipc_add_connection())
        return false;
    }
    char** list_return;
    int count;
    if (Success != Xutf8TextPropertyToTextList(X.display(), &text_prop, &list_return, &count)) {
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
    XDeleteProperty(X.display(), win, X.atom(HERBST_IPC_ARGS_ATOM));
    XChangeProperty(X.display(), win, X.atom(HERBST_IPC_OUTPUT_ATOM),
        X.atom("UTF8_STRING"), 8, PropModeReplace,
        (unsigned char*)output.str().c_str(), 1 + output.str().size());
    // and also set the exit status
    XChangeProperty(X.display(), win, X.atom(HERBST_IPC_STATUS_ATOM),
        XA_ATOM, 32, PropModeReplace, (unsigned char*)&(status), 1);
    // cleanup
    XFreeStringList(list_return);
    XFree(text_prop.value);
    return true;
}

bool IpcServer::isConnectable(Window window) {
    return X.getClass(window) == HERBST_IPC_CLASS;
}

