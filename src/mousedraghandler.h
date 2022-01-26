#pragma once

#include <functional>
#include <memory>

#include "decoration.h"
#include "fixprecdec.h"
#include "rectangle.h"

class Client;
class FrameLeaf;
class FrameSplit;
class HSTag;
class Monitor;
class MonitorManager;
class TagManager;
class ResizeAction;

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
        DragNotPossible(std::string what) : what_(what) {}
        virtual ~DragNotPossible() throw () {}
        virtual const char* what()  const noexcept { return what_.c_str(); }
    private:
        std::string what_;
    };
    //! possibly throws a DragNotPossible exception
    virtual ~MouseDragHandler() {};
    virtual void finalize() = 0;
    virtual void handle_motion_event(Point2D newCursorPos) = 0;

    //! a MouseDragHandler::Constructor creates a MouseDragHandler object, given the
    //! MonitorManager (as a dependency) and the actual client to drag.
    typedef std::function<std::shared_ptr<MouseDragHandler>(MonitorManager*, TagManager*, Client*)> Constructor;

    //! the way in which the current drag handler affects the window dimensions
    ResizeAction resizeAction_;
protected:
    MouseDragHandler() {};
};

/**
 * @brief The MouseDragHandlerFloating class manages the dragging (i.e. mouse
 * moving and resizing) of clients in floating mode
 */
class MouseDragHandlerFloating : public MouseDragHandler {
public:
    typedef void (MouseDragHandlerFloating::*DragFunction)(Point2D);

    MouseDragHandlerFloating(MonitorManager* monitors_, TagManager* tags, Client* dragClient, DragFunction function);
    virtual ~MouseDragHandlerFloating() {};
    virtual void finalize();
    virtual void handle_motion_event(Point2D newCursorPos);
    static Constructor construct(DragFunction dragFunction);
    /* some mouse drag functions */
    void mouse_function_move(Point2D newCursorPos);
    void mouse_function_resize(Point2D newCursorPos);
    void mouse_function_zoom(Point2D newCursorPos);
    bool lockWidth = false; // when resizing, do not modify the width
    bool lockHeight = false; // when resizing, do not modify the height
private:
    void assertDraggingStillSafe();

    MonitorManager*  monitors_;
    TagManager*  tags_;
    Point2D          buttonDragStart_ = {};
    Rectangle        winDragStart_;
    Client*        winDragClient_ = nullptr;
    Monitor*       dragMonitor_ = nullptr;
    unsigned long dragMonitorIndex_ = 0;
    DragFunction dragFunction_ = nullptr;
};


/**
 * @brief The MouseResizeFrame class manages resizing
 * a frame in tiling mode.
 */
class MouseResizeFrame : public MouseDragHandler {
public:
    MouseResizeFrame(MonitorManager* monitors,
                     std::shared_ptr<FrameLeaf> frame,
                     std::weak_ptr<FrameSplit> splitX,
                     std::weak_ptr<FrameSplit> splitY);
    virtual ~MouseResizeFrame() {};
    virtual void finalize();
    virtual void handle_motion_event(Point2D newCursorPos);
    static Constructor construct(std::shared_ptr<FrameLeaf> frame, const ResizeAction& direction);
private:
    void assertDraggingStillSafe();

    MonitorManager*  monitors_;
    Point2D          buttonDragStart_ = {};
    std::weak_ptr<FrameSplit> dragFrameX_; //! the frame whose split is adjusted in x direction
    FixPrecDec       dragStartFractionX_ = FixPrecDec::fromInteger(0); //! initial fraction
    int              dragDistanceUnitX_ = 1; //! 100% split ratio in pixels
    std::weak_ptr<FrameSplit> dragFrameY_; //! the frame whose split is adjusted in y direction
    FixPrecDec       dragStartFractionY_ = FixPrecDec::fromInteger(0); //! initial fraction
    int              dragDistanceUnitY_ = 1; //! 100% split ratio in pixels
    HSTag*           dragTag_; //! the tag containing the dragFrame
    Monitor*         dragMonitor_ = nullptr; //! the monitor with the dragFrame
    unsigned long    dragMonitorIndex_ = 0;
};

