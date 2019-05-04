#ifndef __HERBSTLUFT_IPC_SERVER_H_
#define __HERBSTLUFT_IPC_SERVER_H_

#include <X11/X.h>
#include <functional>
#include <string>
#include <vector>
#include <utility>

class XConnection;

class IpcServer {
public:
    //! a callback that handles a call, represented by a vector of strings. The
    // callback can produce some output and return a status code.
    // This is the counterpart of hc_send_command() in ipc-client/ipc-client.h
    using CallHandler = std::function<std::pair<int,std::string>(const std::vector<std::string>&)>;
    IpcServer(XConnection& xconnection);
    ~IpcServer();
    //! set a function that handle incoming calls. This callback
    //must be set before addConnection() or handleConnection() is called!
    void setCallHandler(CallHandler callback);

    //! check wether window is a ipc client window
    bool isConnectable(Window window);
    //! listen for ipc requests on the given window
    void addConnection(Window win);
    //! try to run an ipc request in the given window, return if there was one
    bool handleConnection(Window window);
    //! send a hook to all listening clients
    void emitHook(std::vector<std::string> args);

private:
    XConnection& X;
    CallHandler callHandler_;

    Window hookEventWindow_; //! window on which the hooks are announced
    int nextHookNumber_; //! index for the next hook
};

#endif

