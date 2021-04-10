#ifndef __HERBSTLUFT_IPC_SERVER_H_
#define __HERBSTLUFT_IPC_SERVER_H_

#include <X11/X.h>
#include <functional>
#include <string>
#include <utility>
#include <vector>

class XConnection;

class IpcServer {
public:
    class CallResult {
    public:
        int exitCode;
        std::string output;
        std::string error;
    };
    //! a callback that handles a call, represented by a vector of strings. The
    // callback can produce some output and return a status code.
    // This is the counterpart of hc_send_command() in ipc-client/ipc-client.h
    using CallHandler = std::function<CallResult(const std::vector<std::string>&)>;
    IpcServer(XConnection& xconnection);
    ~IpcServer();

    //! check wether window is a ipc client window
    bool isConnectable(Window window);
    //! listen for ipc requests on the given window, but don't read them
    //if there are requests already.
    void addConnection(Window win);
    //! try to run an ipc request in the given window via the given callback,
    //return if there was one
    bool handleConnection(Window window, CallHandler callback);
    //! send a hook to all listening clients
    void emitHook(std::vector<std::string> args);

private:
    XConnection& X;

    Window hookEventWindow_; //! window on which the hooks are announced
    int nextHookNumber_; //! index for the next hook
};

#endif

