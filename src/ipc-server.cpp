#include "ipc-server.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdio>
#include <sstream>

#include "command.h"
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

void IpcServer::emitHook(vector<string> args) {
    if (args.size() <= 0) {
        // nothing to do
        return;
    }
    vector<const char*> args_c_str;
    args_c_str.reserve(args.size());
    for (const auto& s : args) {
        args_c_str.push_back(s.c_str());
    }
    XTextProperty text_prop;
    static char atom_name[1000];
    snprintf(atom_name, 1000, HERBST_HOOK_PROPERTY_FORMAT, nextHookNumber_);
    Atom atom = X.atom(atom_name);
    Xutf8TextListToTextProperty(X.display(), (char**) args_c_str.data(), args.size(), XUTF8StringStyle, &text_prop);
    XSetTextProperty(X.display(), hookEventWindow_, &text_prop, atom);
    XFree(text_prop.value);
    // set counter for next property
    nextHookNumber_ += 1;
    nextHookNumber_ %= HERBST_HOOK_PROPERTY_COUNT;
}
