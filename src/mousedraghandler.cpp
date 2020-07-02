#include "mousedraghandler.h"

#include "client.h"
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
    winDragClient_->float_size_ = winDragStart_;
    winDragClient_->float_size_.x += x_diff;
    winDragClient_->float_size_.y += y_diff;
    // snap it to other windows
    int dx, dy;
    client_snap_vector(winDragClient_, dragMonitor_,
                       SNAP_EDGE_ALL, &dx, &dy);
    winDragClient_->float_size_.x += dx;
    winDragClient_->float_size_.y += dy;
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
    // avoid an overflow
    int new_width  = winDragClient_->float_size_.width + x_diff;
    int new_height = winDragClient_->float_size_.height + y_diff;
    Client* client = winDragClient_;
    if (left) {
        winDragClient_->float_size_.x -= x_diff;
    }
    if (top) {
        winDragClient_->float_size_.y -= y_diff;
    }
    winDragClient_->float_size_.width  = new_width;
    winDragClient_->float_size_.height = new_height;
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
        winDragClient_->float_size_.x += dx;
        dx *= -1;
    }
    if (top) {
        winDragClient_->float_size_.y += dy;
        dy *= -1;
    }
    winDragClient_->float_size_.width += dx;
    winDragClient_->float_size_.height += dy;
    client->applysizehints(&winDragClient_->float_size_.width,
                           &winDragClient_->float_size_.height);
    if (left) {
        client->float_size_.x =
            winDragStart_.x + winDragStart_.width
            - winDragClient_->float_size_.width;
    }
    if (top) {
        client->float_size_.y =
            winDragStart_.y + winDragStart_.height
            - winDragClient_->float_size_.height;
    }
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
    client->float_size_.x = cent_x - new_width / 2;
    client->float_size_.y = cent_y - new_height / 2;
    client->float_size_.width = new_width;
    client->float_size_.height = new_height;
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
    client->float_size_.width = new_width;
    client->float_size_.height = new_height;
    client->float_size_.x = cent_x - new_width / 2;
    client->float_size_.y = cent_y - new_height / 2;
    winDragClient_->resize_floating(dragMonitor_, get_current_client() == winDragClient_);
}

MouseResizeFrame::MouseResizeFrame(MonitorManager *monitors, shared_ptr<FrameLeaf> frame)
    : monitors_(monitors)
{
    dragMonitor_ = monitors_->byFrame(frame);
    dragMonitorIndex_ = dragMonitor_->index();
    dragTag_ = dragMonitor_->tag;
    if (!dragMonitor_) {
        throw DragNotPossible("Frame not on any monitor");
    }
    buttonDragStart_ = get_cursor_position();
    Rectangle frameRect = frame->lastRect();
    /* check whether the cursor is the following area:
     *
     * frameRect.tl() -> * -- +
     *                   |\   .
     *                   |A\  .
     *                   |AA\ .
     *                   |AAA\.
     *                   +----* <- frameRect.br()
     *
     */
    bool inBottomLeftTriangle =
        (buttonDragStart_ - frameRect.tl())
            .biggerSlopeThan(frameRect.br() - frameRect.tl());
    /* check whether the cursor is the following area:
     *
     *                   +----* <- frameRect.tr()
     *                   |AAA/.
     *                   |AA/ .
     *                   |A/  .
     *                   |/   .
     * frameRect.bl() -> * -- +
     *
     */
    bool inTopLeftTriangle =
        (frameRect.tr() - frameRect.bl())
            .biggerSlopeThan(buttonDragStart_ - frameRect.bl());
    /* with the combination of the above booleans we know in which of the
     * following four areas the cursor is:
     *
     *    +----*
     *    |\  /|
     *    | \/ |
     *    | /\ |
     *    |/  \|
     *    * -- +
     */
    Direction dir;
    if (inBottomLeftTriangle && inTopLeftTriangle) {
        dir = Direction::Left;
    } else if (inBottomLeftTriangle) {
        dir = Direction::Down;
    } else if (inTopLeftTriangle) {
        dir = Direction::Up;
    } else {
        dir = Direction::Right;
    }
    shared_ptr<Frame> neighbour = frame->neighbour(dir);
    if (!neighbour) {
        throw DragNotPossible("No neighbour frame in the direction of the cursor");
    }

    dragFrame_ = neighbour->getParent();
    auto df = dragFrame_.lock();

    if (df == nullptr) {
        // We need this check because otherwise, -Wnull-dereference complains
        // that a nullptr might be dereferenced:
        throw DragNotPossible("Neighbouring frame has no parent. This should never happen, please report a bug to the developers.");
    }

    dragDistanceUnit_ =
            (df->getAlign() == SplitAlign::vertical)
            ? df->lastRect().height
            : df->lastRect().width;
    dragStartFraction_ = df->getFraction();
}

void MouseResizeFrame::finalize()
{
    assertDraggingStillSafe();
    dragMonitor_->applyLayout();
}

void MouseResizeFrame::handle_motion_event(Point2D newCursorPos)
{
    assertDraggingStillSafe();
    auto df = dragFrame_.lock();

    if (df == nullptr) {
        // This check suppresses a false-positive for -Wnull-dereference
        return;
    }

    auto deltaVec = newCursorPos - buttonDragStart_;
    int delta;
    if (df->getAlign() == SplitAlign::vertical) {
        delta = deltaVec.y;
    } else {
        delta = deltaVec.x;
    }
    // translate delta from 'pixels' to 'FRACTION_UNIT'
    delta = (delta * dragStartFraction_.unit_) / dragDistanceUnit_;
    df->setFraction(dragStartFraction_ + FixPrecDec::raw(delta));
    dragMonitor_->applyLayout();
}

MouseDragHandler::Constructor MouseResizeFrame::construct(shared_ptr<FrameLeaf> frame)
{
    return [frame](MonitorManager* monitors, Client*) {
        return make_shared<MouseResizeFrame>(monitors, frame);
    };
}

void MouseResizeFrame::assertDraggingStillSafe()
{
    bool allFine =
            dragMonitor_
            && dragMonitorIndex_ < monitors_->size()
            && monitors_->byIdx(dragMonitorIndex_) == dragMonitor_
            && dragTag_ == dragMonitor_->tag
            && !dragFrame_.expired();
    if (!allFine) {
        throw DragNotPossible("Monitor, tag or frame disappeared");
    }
}
