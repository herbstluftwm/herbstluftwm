#pragma once

#include <X11/X.h>

class Root;
union _XEvent;
class XConnection;

class XMainLoop {
public:
    XMainLoop(XConnection& X, Root* root);
    void run();
    //! quit the main loop as soon as possible
    void quit();
private:
    using EventHandler = void (XMainLoop::*)(union _XEvent*);
    // members
    XConnection& X_;
    Root* root_;
    bool aboutToQuit_;
    EventHandler handlerTable_[LASTEvent];
    // event handlers
    void buttonpress(union _XEvent* event);
    void buttonrelease(union _XEvent* event);
    void clientmessage(union _XEvent* event);
    void createnotify(union _XEvent* event);
    void configurerequest(union _XEvent* event);
    void configurenotify(union _XEvent* event);
    void destroynotify(union _XEvent* event);
    void enternotify(union _XEvent* event);
    void expose(union _XEvent* event);
    void focusin(union _XEvent* event);
    void keypress(union _XEvent* event);
    void mappingnotify(union _XEvent* event);
    void motionnotify(union _XEvent* event);
    void mapnotify(union _XEvent* event);
    void maprequest(union _XEvent* event);
    void propertynotify(union _XEvent* event);
    void unmapnotify(union _XEvent* event);
};
