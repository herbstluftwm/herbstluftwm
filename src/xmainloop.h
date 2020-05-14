#pragma once

#include <X11/X.h>
#include <X11/Xlib.h>

#include "x11-types.h"

class Root;
class XConnection;

class XMainLoop {
public:
    XMainLoop(XConnection& X, Root* root);
    void scanExistingClients();
    void run();
    //! quit the main loop as soon as possible
    void quit();
    using EventHandler = void (XMainLoop::*)(XEvent*);

    void dropEnterNotifyEvents();
private:
    // members
    XConnection& X_;
    Root* root_;
    bool aboutToQuit_;
    EventHandler handlerTable_[LASTEvent];
    // event handlers
    void buttonpress(XButtonEvent* be);
    void buttonrelease(XButtonEvent* event);
    void clientmessage(XClientMessageEvent* event);
    void createnotify(XCreateWindowEvent* event);
    void configurerequest(XConfigureRequestEvent* cre);
    void configurenotify(XConfigureEvent* event);
    void destroynotify(XUnmapEvent* event);
    void enternotify(XCrossingEvent* ce);
    void expose(XEvent* event);
    void focusin(XEvent* event);
    void keypress(XKeyEvent* event);
    void mappingnotify(XMappingEvent* event);
    void motionnotify(XMotionEvent* event);
    void mapnotify(XMapEvent* event);
    void maprequest(XMapRequestEvent* mapreq);
    void propertynotify(XPropertyEvent* event);
    void unmapnotify(XUnmapEvent* event);

    bool duringEnterNotify_ = false; //! whether we are in enternotify()
};
