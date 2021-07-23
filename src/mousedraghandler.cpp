#include "mousedraghandler.h"

#include "client.h"
#include "decoration.h"
#include "framedata.h"
#include "layout.h"
#include "monitormanager.h"
#include "mouse.h"
#include "x11-utils.h"

using std::make_shared;
using std::shared_ptr;

MouseDragHandlerFloating::MouseDragHandlerFloating(MonitorManager* monitors, Client* dragClient, DragFunction function)
  : monitors_(monitors)
  , winDragClient_(dragClient)
  , dragFunction_(function)
{
    winDragStart_ = winDragClient_->float_size_;
    buttonDragStart_ = get_cursor_position();
    dragMonitor_ = monitors_->byTag(winDragClient_->tag());
    if (dragMonitor_) {
        dragMonitorIndex_ = dragMonitor_->index();
    }
    assertDraggingStillSafe();
}

void MouseDragHandlerFloating::handle_motion_event(Point2D newCursorPos)
{
    assertDraggingStillSafe();
    (this ->* dragFunction_)(newCursorPos);
}

MouseDragHandler::Constructor MouseDragHandlerFloating::construct(DragFunction dragFunction)
{
    return [dragFunction](MonitorManager* monitors, Client* client) -> shared_ptr<MouseDragHandler> {
        return make_shared<MouseDragHandlerFloating>(monitors, client, dragFunction);
    };
}

void MouseDragHandlerFloating::assertDraggingStillSafe() {
    if (!dragMonitor_
            || monitors_->byIdx(dragMonitorIndex_) != dragMonitor_
            || !dragMonitor_
            || !winDragClient_
            || winDragClient_->is_client_floated() == false)
    {
        throw DragNotPossible("Monitor or floating client disappeared");
    }
}

/** finalize what was started by the drag, do more than what is
 * usually done in the destructor.
 * This relayouts the monitor on which the drag happend
 */
void MouseDragHandlerFloating::finalize() {
    assertDraggingStillSafe();
    dragMonitor_->applyLayout();
}

void MouseDragHandlerFloating::mouse_function_move(Point2D newCursorPos) {
    int x_diff = newCursorPos.x - buttonDragStart_.x;
    int y_diff = newCursorPos.y - buttonDragStart_.y;
    // we need to assign it such that the border snapping works
    winDragClient_->float_size_ = winDragStart_.shifted({x_diff, y_diff});
    // snap it to other windows
    int dx, dy;
    client_snap_vector(winDragClient_, dragMonitor_,
                       SNAP_EDGE_ALL, &dx, &dy);
    winDragClient_->float_size_ = winDragClient_->float_size_->shifted({dx, dy});
    winDragClient_->resize_floating(dragMonitor_, get_current_client() == winDragClient_);
}

void MouseDragHandlerFloating::mouse_function_resize(Point2D newCursorPos) {
    int x_diff = newCursorPos.x - buttonDragStart_.x;
    int y_diff = newCursorPos.y - buttonDragStart_.y;
    winDragClient_->float_size_ = winDragStart_;
    // relative x/y coords in drag window
    Monitor* m = dragMonitor_;
    int rel_x = m->relativeX(buttonDragStart_.x) - winDragStart_.x;
    int rel_y = m->relativeY(buttonDragStart_.y) - winDragStart_.y;
    bool top = false;
    bool left = false;
    if (rel_y < winDragStart_.height/2) {
        top = true;
        y_diff *= -1;
    }
    if (rel_x < winDragStart_.width/2) {
        left = true;
        x_diff *= -1;
    }
    if (lockWidth) {
        x_diff = 0;
        if (winDragClient_->mina_ > 0 || winDragClient_->maxa_ > 0) {
            // if the client requires an aspect ratio, (e.g. mpv)
            // then just keep current aspect ratio of the window
            x_diff = (y_diff * winDragStart_.width) / winDragStart_.height;
        }
    }
    if (lockHeight) {
        y_diff = 0;
        if (winDragClient_->mina_ > 0 || winDragClient_->maxa_ > 0) {
            // if the client requires an aspect ratio, (e.g. mpv)
            // then just keep current aspect ratio of the window
            y_diff = (x_diff * winDragStart_.height) / winDragStart_.width;
        }
    }
    // avoid an overflow
    int new_width  = winDragClient_->float_size_->width + x_diff;
    int new_height = winDragClient_->float_size_->height + y_diff;
    Client* client = winDragClient_;
    Rectangle new_geometry = winDragClient_->float_size_;
    if (left) {
        new_geometry.x -= x_diff;
    }
    if (top) {
        new_geometry.y -= y_diff;
    }
    new_geometry.width = new_width;
    new_geometry.height = new_height;
    winDragClient_->float_size_ = new_geometry;
    // snap it to other windows
    int dx, dy;
    int snap_flags = 0;
    if (left) {
        snap_flags |= SNAP_EDGE_LEFT;
    } else {
        snap_flags |= SNAP_EDGE_RIGHT;
    }
    if (top) {
        snap_flags |= SNAP_EDGE_TOP;
    } else {
        snap_flags |= SNAP_EDGE_BOTTOM;
    }
    client_snap_vector(winDragClient_, dragMonitor_,
                       (SnapFlags)snap_flags, &dx, &dy);
    if (left) {
        new_geometry.x += dx;
        dx *= -1;
    }
    if (top) {
        new_geometry.y += dy;
        dy *= -1;
    }
    new_geometry.width += dx;
    new_geometry.height += dy;
    client->applysizehints(&new_geometry.width,
                           &new_geometry.height);
    if (left) {
        new_geometry.x =
            winDragStart_.x + winDragStart_.width
            - new_geometry.width;
    }
    if (top) {
        new_geometry.y =
            winDragStart_.y + winDragStart_.height
            - new_geometry.height;
    }
    winDragClient_->float_size_ = new_geometry;
    winDragClient_->resize_floating(dragMonitor_, get_current_client() == winDragClient_);
}

void MouseDragHandlerFloating::mouse_function_zoom(Point2D newCursorPos) {
    // stretch, where center stays at the same position
    int x_diff = newCursorPos.x - buttonDragStart_.x;
    int y_diff = newCursorPos.y - buttonDragStart_.y;
    // relative x/y coords in drag window
    Monitor* m = dragMonitor_;
    int rel_x = m->relativeX(buttonDragStart_.x) - winDragStart_.x;
    int rel_y = m->relativeY(buttonDragStart_.y) - winDragStart_.y;
    int cent_x = winDragStart_.x + winDragStart_.width  / 2;
    int cent_y = winDragStart_.y + winDragStart_.height / 2;
    if (rel_x < winDragStart_.width/2) {
        x_diff *= -1;
    }
    if (rel_y < winDragStart_.height/2) {
        y_diff *= -1;
    }
    Client* client = winDragClient_;

    // avoid an overflow
    int new_width  = winDragStart_.width  + 2 * x_diff;
    int new_height = winDragStart_.height + 2 * y_diff;
    // apply new rect
    client->float_size_ = Rectangle(cent_x - new_width / 2,
                                    cent_y - new_height / 2,
                                    new_width,
                                    new_height);
    // snap it to other windows
    int right_dx, bottom_dy;
    int left_dx, top_dy;
    // we have to distinguish the direction in which we zoom
    client_snap_vector(winDragClient_, m,
                     (SnapFlags)(SNAP_EDGE_BOTTOM | SNAP_EDGE_RIGHT), &right_dx, &bottom_dy);
    client_snap_vector(winDragClient_, m,
                       (SnapFlags)(SNAP_EDGE_TOP | SNAP_EDGE_LEFT), &left_dx, &top_dy);
    // e.g. if window snaps by vector (3,3) at topleft, window has to be shrinked
    // but if the window snaps by vector (3,3) at bottomright, window has to grow
    if (abs(right_dx) < abs(left_dx)) {
        right_dx = -left_dx;
    }
    if (abs(bottom_dy) < abs(top_dy)) {
        bottom_dy = -top_dy;
    }
    new_width += 2 * right_dx;
    new_height += 2 * bottom_dy;
    client->applysizehints(&new_width, &new_height);
    // center window again
    client->float_size_ = Rectangle(cent_x - new_width / 2,
                                    cent_y - new_height / 2,
                                    new_width,
                                    new_height);
    winDragClient_->resize_floating(dragMonitor_, get_current_client() == winDragClient_);
}

MouseResizeFrame::MouseResizeFrame(MonitorManager *monitors, shared_ptr<FrameLeaf> frame,
                     std::weak_ptr<FrameSplit> splitX,
                     std::weak_ptr<FrameSplit> splitY)
    : monitors_(monitors)
    , dragFrameX_(splitX)
    , dragFrameY_(splitY)
{
    dragMonitor_ = monitors_->byFrame(frame);
    dragMonitorIndex_ = dragMonitor_->index();
    dragTag_ = dragMonitor_->tag;
    if (!dragMonitor_) {
        throw DragNotPossible("Frame not on any monitor");
    }
    buttonDragStart_ = get_cursor_position();
    auto dfX = dragFrameX_.lock();
    if (dfX != nullptr) {
        dragDistanceUnitX_ = dfX->lastRect().width;
        dragStartFractionX_ = dfX->getFraction();
    }
    auto dfY = dragFrameY_.lock();
    if (dfY != nullptr) {
        dragDistanceUnitY_ = dfY->lastRect().height;
        dragStartFractionY_ = dfY->getFraction();
    }
}

void MouseResizeFrame::finalize()
{
    assertDraggingStillSafe();
    dragMonitor_->applyLayout();
}

void MouseResizeFrame::handle_motion_event(Point2D newCursorPos)
{
    assertDraggingStillSafe();
    auto dfX = dragFrameX_.lock();
    auto dfY = dragFrameY_.lock();

    auto deltaVec = newCursorPos - buttonDragStart_;
    // translate deltaVec from 'pixels' to 'FRACTION_UNIT'
    if (dfX) {
        int delta = (deltaVec.x * dragStartFractionX_.unit_) / dragDistanceUnitX_;
        dfX->setFraction(dragStartFractionX_ + FixPrecDec::raw(delta));
    }
    if (dfY) {
        int delta = (deltaVec.y * dragStartFractionY_.unit_) / dragDistanceUnitY_;
        dfY->setFraction(dragStartFractionY_ + FixPrecDec::raw(delta));
    }
    dragMonitor_->applyLayout();
}

MouseDragHandler::Constructor MouseResizeFrame::construct(shared_ptr<FrameLeaf> frame, const ResizeAction& resize)
{
    // a helper function to find a split in a certain direction.
    // for simpler compilation, frame is passed as an argument
    // and not as a closure / variable capture.
    auto splitInDirection =
    [](shared_ptr<FrameLeaf>& frameStart, Direction dir) -> shared_ptr<FrameSplit> {
        shared_ptr<Frame> neighbour = frameStart->neighbour(dir);
        if (neighbour) {
            return neighbour->getParent();
        }
        return {};
    };
    shared_ptr<FrameSplit> horizontalSplit = {};
    shared_ptr<FrameSplit> verticalSplit = {};
    if (resize.left) {
        horizontalSplit = splitInDirection(frame, Direction::Left);
    }
    if (resize.right) {
        horizontalSplit = splitInDirection(frame, Direction::Right);
    }
    if (resize.top) {
        verticalSplit = splitInDirection(frame, Direction::Up);
    }
    if (resize.bottom) {
        verticalSplit = splitInDirection(frame, Direction::Down);
    }
    return [frame, horizontalSplit, verticalSplit](MonitorManager* monitors, Client*) {
        return make_shared<MouseResizeFrame>(monitors, frame, horizontalSplit, verticalSplit);
    };
}

void MouseResizeFrame::assertDraggingStillSafe()
{
    bool allFine =
            dragMonitor_
            && dragMonitorIndex_ < monitors_->size()
            && monitors_->byIdx(dragMonitorIndex_) == dragMonitor_
            && dragTag_ == dragMonitor_->tag
            && (!dragFrameX_.expired() || !dragFrameY_.expired());
    if (!allFine) {
        throw DragNotPossible("Monitor, tag or frame disappeared");
    }
}
