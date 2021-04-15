#include "ipc-server.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdio>

#include "ipc-protocol.h"
#include "xconnection.h"

using std::string;
using std::vector;

IpcServer::IpcServer(XConnection& xconnection)
    : X(xconnection)
    , nextHookNumber_(0)
{
    // main task of the construtor is to setup the hook window
    hookEventWindow_ = XCreateSimpleWindow(X.display(), X.root(),
                                             42, 42, 42, 42, 0, 0, 0);
    // set wm_class for window
    XClassHint *hint = XAllocClassHint();
    hint->res_name = (char*)HERBST_HOOK_CLASS;
    hint->res_class = (char*)HERBST_HOOK_CLASS;
    XSetClassHint(X.display(), hookEventWindow_, hint);
    XFree(hint);
    // ignore all events for this window
    XSelectInput(X.display(), hookEventWindow_, 0l);
    // set its window id in root window
    XChangeProperty(X.display(), X.root(), X.atom(HERBST_HOOK_WIN_ID_ATOM),
        XA_ATOM, 32, PropModeReplace, (unsigned char*)&hookEventWindow_, 1);
}

IpcServer::~IpcServer() {
    // remove property from root window
    XDeleteProperty(X.display(), X.root(), X.atom(HERBST_HOOK_WIN_ID_ATOM));
    XDestroyWindow(X.display(), hookEventWindow_);
}

void IpcServer::addConnection(Window window) {
    XSelectInput(X.display(), window, PropertyChangeMask);
}

bool IpcServer::handleConnection(Window win, CallHandler callback) {
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
    vector<string> arguments;
    for (int i = 0; i < count; i++) {
        arguments.push_back(list_return[i]);
    }
    auto result = callback(arguments);
    // send output back
    int status = result.exitCode;
    string output;
    // concatenate 'normal' and error output
    if (!result.error.empty()) {
        output = result.error;
        // ensure that the error output ends with a newline
        if (*result.error.rbegin() != '\n') {
            output += "\n";
        }
    }
    output += result.output;
    // Mark this command as executed
    XDeleteProperty(X.display(), win, X.atom(HERBST_IPC_ARGS_ATOM));
    X.setPropertyString(win, X.atom(HERBST_IPC_OUTPUT_ATOM), output);
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

void IpcServer::emitHook(vector<string> args) {
    if (args.empty()) {
        // nothing to do
        return;
    }
    static char atom_name[1000];
    snprintf(atom_name, 1000, HERBST_HOOK_PROPERTY_FORMAT, nextHookNumber_);
    X.setPropertyString(hookEventWindow_, X.atom(atom_name), args);
    // set counter for next property
    nextHookNumber_ += 1;
    nextHookNumber_ %= HERBST_HOOK_PROPERTY_COUNT;
}
