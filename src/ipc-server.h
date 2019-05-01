#ifndef __HERBSTLUFT_IPC_SERVER_H_
#define __HERBSTLUFT_IPC_SERVER_H_

#include <X11/X.h>
#include <string>
#include <vector>

class XConnection;

class IpcServer {
public:
    IpcServer(XConnection& xconnection);
    ~IpcServer();

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

    Window hook_event_window_; //! window on which the hooks are announced
    int next_hook_number_; //! index for the next hook
};

#endif

