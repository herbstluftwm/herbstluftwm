#pragma once

#include <exception>
#include <functional>
#include <memory>

#include "x11-types.h"

class Client;
class Monitor;
class MonitorManager;
class MouseDragHandlerFloating;

typedef void (MouseDragHandlerFloating::*MouseDragFunction)(Point2D);

/**
 * @brief The abstract class MouseDragHandler encapsulates what drag handling
 should do: after starting, it is fed with new coordinates whenever the mouse
 cursor moves. This is done until mouse dragging stops or until the dragged
 resources disappear.
 */
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
    virtual ~MouseDragHandler() {};
    virtual void finalize() = 0;
    virtual void handle_motion_event(Point2D newCursorPos) = 0;

    //! a MouseDragHandler::Constructor creates a MouseDragHandler object, given the
    //! MonitorManager (as a dependency) and the actual client to drag.
    typedef std::function<std::shared_ptr<MouseDragHandler>(MonitorManager*, Client*)> Constructor;
protected:
    MouseDragHandler() {};
};

/**
 * @brief The MouseDragHandlerFloating class manages the dragging (i.e. mouse
 * moving and resizing) of clients in floating mode
 */
class MouseDragHandlerFloating : public MouseDragHandler {
public:
    MouseDragHandlerFloating(MonitorManager* monitors_, Client* dragClient, MouseDragFunction function);
    virtual ~MouseDragHandlerFloating() {};
    virtual void finalize();
    virtual void handle_motion_event(Point2D newCursorPos);
    static Constructor construct(MouseDragFunction dragFunction);
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

