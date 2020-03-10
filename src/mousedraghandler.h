#pragma once

#include <exception>

#include "x11-types.h"

class Client;
class Monitor;
class MonitorManager;
class MouseDragHandler;

typedef void (MouseDragHandler::*MouseDragFunction)(Point2D);

class MouseDragHandler {
public:
    class DragNotPossible : virtual public std::exception {
    public:
        /** the exception expresses that dragging the client is not possible or
         * not possible anymore. Ususally this happens, when some of the objects
         * involved (monitor, client, tag, frame) change or get destroyed. */
        DragNotPossible() {}
        virtual ~DragNotPossible() throw () {}
    };
    //! possibly throws a DragNotPossible exception
    MouseDragHandler(MonitorManager* monitors_, Client* dragClient, MouseDragFunction function);
    void finalize();
    void handle_motion_event(Point2D newCursorPos);

    /* some mouse drag functions */
    void mouse_function_move(Point2D newCursorPos);
    void mouse_function_resize(Point2D newCursorPos);
    void mouse_function_zoom(Point2D newCursorPos);
private:
    void assertDraggingStillSafe();

    MonitorManager*  monitors_;
    Point2D          buttonDragStart_;
    Rectangle        winDragStart_;
    Client*        winDragClient_ = nullptr;
    Monitor*       dragMonitor_ = nullptr;
    unsigned long dragMonitorIndex_ = 0;
    MouseDragFunction dragFunction_ = nullptr;
};

