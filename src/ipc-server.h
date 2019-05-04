#ifndef __HERBSTLUFT_IPC_SERVER_H_
#define __HERBSTLUFT_IPC_SERVER_H_

#include <X11/X.h>

class XConnection;

class IpcServer {
public:
    IpcServer(XConnection& xconnection);

    //! check wether window is a ipc client window
    bool isConnectable(Window window);
    //! listen for ipc requests on the given window
    void addConnection(Window win);
    //! try to run an ipc request in the given window, return if there was one
    bool handleConnection(Window window);

private:
    XConnection& X;
};

#endif

