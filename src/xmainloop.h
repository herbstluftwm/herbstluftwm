#pragma once

#include <X11/X.h>
#include <X11/Xlib.h>

class Root;
class XConnection;

class XMainLoop {
public:
    XMainLoop(XConnection& X, Root* root);
    void run();
    //! quit the main loop as soon as possible
    void quit();
private:
    using EventHandler = void (XMainLoop::*)(XEvent*);
    // members
    XConnection& X_;
    Root* root_;
    bool aboutToQuit_;
    EventHandler handlerTable_[LASTEvent];
    // event handlers
    void buttonpress(XEvent* event);
    void buttonrelease(XEvent* event);
    void clientmessage(XEvent* event);
    void createnotify(XEvent* event);
    void configurerequest(XEvent* event);
    void configurenotify(XEvent* event);
    void destroynotify(XEvent* event);
    void enternotify(XEvent* event);
    void expose(XEvent* event);
    void focusin(XEvent* event);
    void keypress(XEvent* event);
    void mappingnotify(XEvent* event);
    void motionnotify(XEvent* event);
    void mapnotify(XEvent* event);
    void maprequest(XEvent* event);
    void propertynotify(XEvent* event);
    void unmapnotify(XEvent* event);
};
