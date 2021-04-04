#include "monitor.h"

#include <X11/Xlib.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <sstream>
#include <vector>

#include "client.h"
#include "clientmanager.h"
#include "completion.h"
#include "ewmh.h"
#include "floating.h"
#include "frametree.h"
#include "globals.h"
#include "hook.h"
#include "ipc-protocol.h"
#include "layout.h"
#include "monitormanager.h"
#include "root.h"
#include "settings.h"
#include "stack.h"
#include "tag.h"
#include "tagmanager.h"
#include "utils.h"

using std::endl;
using std::string;
using std::stringstream;
using std::vector;

extern MonitorManager* g_monitors;

Monitor::Monitor(Settings* settings_, MonitorManager* monman_, Rectangle rect_, HSTag* tag_)
    : tag(tag_)
    , tag_previous(tag_)
    , name      (this, "name", "",
           [monman_](string n) { return monman_->isValidMonitorName(n); })
    , index     (this, "index", 0)
    , tag_string(this, "tag", &Monitor::getTagString, &Monitor::setTagString)
    , pad_up    (this, "pad_up", 0)
    , pad_right (this, "pad_right", 0)
    , pad_down  (this, "pad_down", 0)
    , pad_left  (this, "pad_left", 0)
    , lock_tag  (this, "lock_tag", false)
    , pad_automatically_set ({false, false, false, false})
    , dirty(true)
    , lock_frames(false)
    , mouse { 0, 0 }
    , rect(this, "geometry", rect_, &Monitor::atLeastMinWindowSize)
    , settings(settings_)
    , monman(monman_)
{
    // explicitly set members writable such that gendoc.py recognizes it
    pad_up.setWritable();
    pad_right.setWritable();
    pad_down.setWritable();
    pad_left.setWritable();
    lock_tag.setWritable();
    for (auto i : {&pad_up, &pad_left, &pad_right, &pad_down}) {
        i->changed().connect(this, &Monitor::applyLayout);
    }
    rect.changedByUser().connect(this, &Monitor::applyLayout);

    rect.setDoc("the outer geometry of the monitor");

    stacking_window = XCreateSimpleWindow(g_display, g_root,
                                             42, 42, 42, 42, 1, 0, 0);
    setDoc("The monitor is a rectangular part on the screen that holds "
           "precisely one tag at a time. The pad attributes reserve "
           "space on the monitor\'s edge for panels, so this space "
           "(given in number of pixels) is never occupied by tiled clients.");
    name.setDoc("the monitor\'s name (can be empty)");
    index.setDoc("the monitor\'s index (starts at index 0)");
    tag_string.setDoc("the name of the tag viewed here");
    pad_up.setDoc("space for panels at the monitor\'s upper edge");
    pad_right.setDoc("space for panels at the monitor\'s right edge");
    pad_down.setDoc("space for panels at the monitor\'s lower edge");
    pad_left.setDoc("space for panels at the monitor\'s left edge");
    lock_tag.setDoc("if activated, then it it is not possible to switch "
                    "this monitor to a different tag.");
}

Monitor::~Monitor() {
    XDestroyWindow(g_display, stacking_window);
}

string Monitor::getTagString() {
    return tag->name();
}

string Monitor::setTagString(string new_tag_string) {
    HSTag* new_tag = find_tag(new_tag_string.c_str());
    if (!new_tag) {
        return "no tag named \"" + new_tag_string + "\" exists.";
    }
    if (new_tag == tag) {
        return ""; // nothing to do
    }
    bool success = this->setTag(new_tag);
    if (!success) {
        return "tag \"" + new_tag_string + "\" is already on another monitor";
        /* Note: To change this to tag-swapping between monitors, implement a method
         * MonitorManager::stealTag() that will fetch the corresponding monitor
         * and perform the swap */
    }
    return "to be implemented"; // TODO: implement in setTag()
}

void Monitor::setIndexAttribute(unsigned long new_index) {
    index = new_index;
}

int Monitor::lock_tag_cmd(Input, Output) {
    lock_tag = true;
    return 0;
}

int Monitor::unlock_tag_cmd(Input, Output) {
    lock_tag = false;
    return 0;
}

void Monitor::noComplete(Completion &complete)
{
    complete.none();
}

int Monitor::list_padding(Input, Output output) {
    output     << pad_up()
        << " " << pad_right()
        << " " << pad_down()
        << " " << pad_left()
        << "\n";
    return 0;
}

/** Set the tag shown on the monitor.
 * Return false if tag is already shown on another monitor.
 */
// TODO this is the job of monitormanager
bool Monitor::setTag(HSTag* new_tag) {
    auto owner = find_monitor_with_tag(new_tag);
    if (!owner || owner != this) {
        // TODO do the work!
        return true;
    }
    return owner == this;
}

void Monitor::applyLayout() {
    if (settings->monitors_locked) {
        dirty = true;
        return;
    }
    dirty = false;
    Rectangle cur_rect = rect;
    // apply pad
    // FIXME: why does the following + work for attributes pad_* ?
    cur_rect.x += pad_left();
    cur_rect.width -= (pad_left() + pad_right());
    cur_rect.y += pad_up();
    cur_rect.height -= (pad_up() + pad_down());
    if (!g_settings->smart_frame_surroundings() || tag->frame->root_->isSplit()) {
        // apply frame gap
        cur_rect.x += settings->frame_gap();
        cur_rect.y += settings->frame_gap();
        cur_rect.height -= settings->frame_gap();
        cur_rect.width -= settings->frame_gap();
    }
    bool isFocused = get_current_monitor() == this;
    TilingResult res = tag->frame->root_->computeLayout(cur_rect);
    if (tag->floating_focused) {
        res.focus = tag->focusedClient();
    }
    if (tag->floating) {
        for (auto& p : res.data) {
            p.second.floated = true;
            // deactivate smart_window_surroundings in floating mode
            p.second.minimalDecoration = false;
        }
    }
    // preprocessing
    for (auto& p : res.data) {
        if (p.first->fullscreen_() || p.second.floated) {
            // do not hide fullscreen windows
            p.second.visible = true;
        }
        if (settings->hide_covered_windows) {
            // apply hiding of windows: move them to out of the screen:
            if (!p.second.visible) {
                Rectangle& geo = p.second.geometry;
                geo.x = -100 - geo.width;
                geo.y = -100 - geo.height;
            }
        }
    }
    // 1. Update stack (TODO: why stack first?)
    for (auto& p : res.data) {
        Client* c = p.first;
        if (c->fullscreen_()) {
            tag->stack->sliceAddLayer(c->slice, LAYER_FULLSCREEN);
        } else {
            tag->stack->sliceRemoveLayer(c->slice, LAYER_FULLSCREEN);
        }
        // special raise rules for tiled clients:
        if (!p.second.floated) {
            // this client is the globally focused client if this monitor
            // is focused and if this client is the focus on this monitor:
            bool globallyFocusedClient = isFocused && res.focus == c;
            // if this client is globally focused, then it has a different border color
            // and so we raise it to make the look of overlapping shadows more pleasent if
            // a compositor is running.
            //
            // Thus, raise this client if it needs to be raised according
            // to the TilingResult (if it is the selected window in a max-frame) or
            // if this client is focused:
            if (p.second.needsRaise || globallyFocusedClient) {
                c->raise();
            }
        }
    }
    tag->stack->clearLayer(LAYER_FOCUS);
    if (res.focus) {
        // activate the focus layer if requested by the setting
        // or if there is a fullscreen client potentially covering
        // the focused client.
        if ((isFocused && g_settings->raise_on_focus_temporarily())
            || tag->stack->isLayerEmpty(LAYER_FULLSCREEN) == false)
        {
            tag->stack->sliceAddLayer(res.focus->slice, LAYER_FOCUS);
        }
    }
    restack();
    // 2. Update window geometries
    for (auto& p : res.data) {
        Client* c = p.first;
        bool clientFocused = isFocused && res.focus == c;
        if (c->fullscreen_()) {
            c->resize_fullscreen(rect, clientFocused);
        } else if (p.second.floated) {
            c->resize_floating(this, clientFocused);
        } else {
            bool minDec = p.second.minimalDecoration;
            c->resize_tiling(p.second.geometry, clientFocused, minDec);
        }
    }
    for (auto& c : tag->floating_clients_) {
        if (c->fullscreen_()) {
            c->resize_fullscreen(rect, res.focus == c && isFocused);
        } else {
            c->resize_floating(this, res.focus == c && isFocused);
        }
    }
    if (tag->floating) {
        for (auto& p : res.frames) {
            p.first->hide();
        }
    } else {
        for (auto& p : res.frames) {
            p.first->render(p.second, p.first == res.focused_frame && isFocused);
            p.first->updateVisibility(p.second, p.first == res.focused_frame && isFocused);
        }
    }
    if (isFocused) {
        if (res.focus) {
            Root::get()->clients()->focus = res.focus;
            res.focus->window_focus();
        } else {
            Root::get()->clients()->focus = {};
            Client::window_unfocus_last();
        }
    }

    // remove all enternotify-events from the event queue that were
    // generated while arranging the clients on this monitor
    monman->dropEnterNotifyEvents.emit();
}

Monitor* find_monitor_by_name(const char* name) {
    for (auto m : *g_monitors) {
        if (m->name == name) {
            return m;
        }
    }
    return nullptr;
}

Monitor* string_to_monitor(const char* str) {
  return g_monitors->byString(str);
}


int Monitor::move_cmd(Input input, Output output) {
    // usage: move_monitor INDEX RECT [PADUP [PADRIGHT [PADDOWN [PADLEFT]]]]
    // moves monitor with number to RECT
    if (input.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto new_rect = Rectangle::fromStr(input.front());
    if (new_rect.width < WINDOW_MIN_WIDTH || new_rect.height < WINDOW_MIN_HEIGHT) {
        output << input.command() << ": Rectangle is too small\n";
        return HERBST_INVALID_ARGUMENT;
    }
    // else: just move it:
    this->rect = new_rect;
    input.shift();
    if (!input.empty()) {
        pad_up.change(input.front());
    }
    input.shift();
    if (!input.empty()) {
        pad_right.change(input.front());
    }
    input.shift();
    if (!input.empty()) {
        pad_down.change(input.front());
    }
    input.shift();
    if (!input.empty()) {
        pad_left.change(input.front());
    }
    monitorMoved.emit();
    applyLayout();
    return 0;
}

void Monitor::move_complete(Completion& complete) {
    if (complete == 1) {
        complete.full(Converter<Rectangle>::str(rect));
    } else if (complete >= 2 && complete <= 5) {
        vector<Attribute_<int>*> pads =
            { &pad_up, &pad_right, &pad_down, &pad_left };
        complete.full("0");
        size_t idx = complete.needleIndex();
        pads[idx - 2]->complete(complete);
    } else {
        complete.none();
    }
}

int Monitor::renameCommand(Input input, Output output) {
    string new_name;
    if (!(input >> new_name)) {
        return HERBST_NEED_MORE_ARGS;
    }
    string error = name.change(new_name);
    if (!error.empty()) {
        output << input.command() << ": " << error << "\n";
        return HERBST_INVALID_ARGUMENT;
    } else {
        return 0;
    }
}

void Monitor::renameComplete(Completion& complete)
{
   if (complete == 1) {
       // no completion, because the completion of the
       // converter only suggests the current name anyway.
   } else if (complete >= 2) {
       complete.none();
   }
}

Monitor* find_monitor_with_tag(HSTag* tag) {
    for (auto m : *g_monitors) {
        if (m->tag == tag) {
            return m;
        }
    }
    return nullptr;
}

Monitor* get_current_monitor() {
    return g_monitors->byIdx(g_monitors->cur_monitor);
}

void all_monitors_apply_layout() {
    for (auto m : *g_monitors) {
        m->applyLayout();
    }
}

int monitor_set_tag(Monitor* monitor, HSTag* tag) {
    Monitor* other = find_monitor_with_tag(tag);
    if (monitor == other) {
        // nothing to do
        return 0;
    }
    if (monitor->lock_tag) {
        // If the monitor tag is locked, do not change the tag
        if (other) {
            // but if the tag is already visible, change to the
            // displaying monitor
            monitor_focus_by_index(other->index());
            return 0;
        }
        return 1;
    }
    if (other) {
        if (g_settings->swap_monitors_to_get_tag()) {
            if (other->lock_tag) {
                // the monitor we want to steal the tag from is
                // locked. focus that monitor instead
                monitor_focus_by_index(other->index());
                return 0;
            }
            monitor->tag_previous = monitor->tag;
            other->tag_previous = other->tag;
            // swap tags
            other->tag = monitor->tag;
            monitor->tag = tag;
            /* TODO: find the best order of restacking and layouting */
            other->restack();
            monitor->restack();
            other->applyLayout();
            monitor->applyLayout();
            monitor_update_focus_objects();
            Ewmh::get().updateCurrentDesktop();
            emit_tag_changed(other->tag, other->index());
            emit_tag_changed(tag, g_monitors->cur_monitor);
        } else {
            // if we are not allowed to steal the tag, then just focus the
            // other monitor
            monitor_focus_by_index(other->index());
        }
        return 0;
    }
    HSTag* old_tag = monitor->tag;
    // save old tag
    monitor->tag_previous = old_tag;
    // 1. show new tag
    monitor->tag = tag;
    // first reset focus and arrange windows
    monitor->restack();
    monitor->lock_frames = true;
    monitor->applyLayout();
    monitor->lock_frames = false;
    // then show them (should reduce flicker)
    tag->setVisible(true);
    if (!monitor->tag->floating) {
        // monitor->tag->frame->root_->updateVisibility();
    }
    // 2. hide old tag
    old_tag->setVisible(false);
    // focus window just has been shown
    // discard enternotify-events
    g_monitors->dropEnterNotifyEvents.emit();
    monitor_update_focus_objects();
    Ewmh::get().updateCurrentDesktop();
    emit_tag_changed(tag, g_monitors->cur_monitor);
    return 0;
}

void monitor_focus_by_index(unsigned new_selection) {
    // clamp to last
    new_selection = std::min(g_monitors->size() - 1, (size_t)new_selection);
    Monitor* old = get_current_monitor();
    Monitor* monitor = g_monitors->byIdx(new_selection);
    if (old == monitor) {
        // nothing to do
        return;
    }
    // change selection globals
    assert(monitor->tag);
    assert(monitor->tag->frame->root_);
    g_monitors->cur_monitor = new_selection;
    // repaint g_monitors
    old->applyLayout();
    monitor->applyLayout();
    int rx, ry;
    {
        // save old mouse position
        Window win, child;
        int wx, wy;
        unsigned int mask;
        if (True == XQueryPointer(g_display, g_root, &win, &child,
            &rx, &ry, &wx, &wy, &mask)) {
            old->mouse.x = rx - old->rect->x;
            old->mouse.y = ry - old->rect->y;
            old->mouse.x = CLAMP(old->mouse.x, 0, old->rect->width-1);
            old->mouse.y = CLAMP(old->mouse.y, 0, old->rect->height-1);
        }
    }
    // restore position of new monitor
    // but only if mouse pointer is not already on new monitor
    int new_x, new_y;
    if ((monitor->rect->x <= rx) && (rx < monitor->rect->x + monitor->rect->width)
        && (monitor->rect->y <= ry) && (ry < monitor->rect->y + monitor->rect->height)) {
        // mouse already is on new monitor
    } else {
        // If the mouse is located in a gap indicated by
        // mouse_recenter_gap at the outer border of the monitor,
        // recenter the mouse.
        if (std::min(monitor->mouse.x, abs(monitor->mouse.x - monitor->rect->width))
                < g_settings->mouse_recenter_gap()
            || std::min(monitor->mouse.y, abs(monitor->mouse.y - monitor->rect->height))
                < g_settings->mouse_recenter_gap()) {
            monitor->mouse.x = monitor->rect->width / 2;
            monitor->mouse.y = monitor->rect->height / 2;
        }
        new_x = monitor->rect->x + monitor->mouse.x;
        new_y = monitor->rect->y + monitor->mouse.y;
        XWarpPointer(g_display, None, g_root, 0, 0, 0, 0, new_x, new_y);
        // discard all mouse events caused by this pointer movage from the
        // event queue, so the focus really stays in the last focused window on
        // this monitor and doesn't jump to the window hovered by the mouse
        g_monitors->dropEnterNotifyEvents.emit();
    }
    // update objects
    monitor_update_focus_objects();
    // emit hooks
    Ewmh::get().updateCurrentDesktop();
    emit_tag_changed(monitor->tag, new_selection);
}

void monitor_update_focus_objects() {
    g_monitors->focus = g_monitors->byIdx(g_monitors->cur_monitor);
    global_tags->updateFocusObject(g_monitors->focus());
}

int Monitor::relativeX(int x_root) {
    return x_root - rect->x - pad_left;
}

int Monitor::relativeY(int y_root) {
    return y_root - rect->y - pad_up;
}

void Monitor::restack() {
    Window fullscreenFocus = 0;
    /* don't add a focused fullscreen client to the stack because
     * we want a focused fullscreen window to be above the panels which are
     * usually unmanaged. All the windows passed to the XRestackWindows
     * will end up below all unmanaged windows, so don't add a focused
     * fullscreen window to it. Instead raise the fullscreen window
     * manually such that it is above the panel */
    Client* client = tag->focusedClient();
    if (client && client->fullscreen_) {
        fullscreenFocus = client->decorationWindow();
        XRaiseWindow(g_display, fullscreenFocus);
    }
    // collect all other windows in a vector and pass it to XRestackWindows
    vector<Window> buf = { stacking_window };
    auto addToVector = [&buf, fullscreenFocus](Window w) {
        if (w != fullscreenFocus) {
            buf.push_back(w);
        }
    };
    tag->stack->extractWindows(false, addToVector);
    XRestackWindows(g_display, buf.data(), buf.size());
}

Rectangle Monitor::getFloatingArea() const {
    auto m = this;
    auto r = m->rect();
    r.x += m->pad_left;
    r.width -= m->pad_left + m->pad_right;
    r.y += m->pad_up;
    r.height -= m->pad_up + m->pad_down;
    return r;
}

//! Returns a textual description of the monitor
string Monitor::getDescription() {
    stringstream label;
    label << "Monitor " << index();
    if (!name().empty()) {
        label << " (\"" << name() << "\")";
    }
    label << " with tag \"" << tag->name() << "\"";
    return label.str();
}

void Monitor::evaluateClientPlacement(Client* client, ClientPlacement placement) const
{
    switch (placement) {
        case ClientPlacement::Center:
            {
                Point2D new_tl =
                    // the center of the monitor
                    getFloatingArea().dimensions() / 2
                    // minus half the dimensions of the client
                    - client->float_size_->dimensions() / 2;
                client->float_size_ = Rectangle(new_tl.x, new_tl.y,
                                                client->float_size_->width,
                                                client->float_size_->height);
            }
            break;

        case ClientPlacement::Smart:
            {
                Point2D area = getFloatingArea().dimensions();
                Point2D new_tl = Floating::smartPlacement(tag, client,
                                             area, settings->snap_gap);
                client->float_size_ = Rectangle(new_tl.x, new_tl.y,
                                                client->float_size_->width,
                                                client->float_size_->height);
            }
            break;

        case ClientPlacement::Unchanged:
            // do not do anything
            break;
    }
}

string Monitor::atLeastMinWindowSize(Rectangle geom)
{
    if (geom.width < WINDOW_MIN_WIDTH) {
        return "Rectangle too small; it must be at least "
                + Converter<int>::str(WINDOW_MIN_WIDTH) + " wide.";
    }
    if (geom.height < WINDOW_MIN_HEIGHT) {
        return "Rectangle too small; it must be at least "
                + Converter<int>::str(WINDOW_MIN_HEIGHT) + " high.";
    }
    return {};
}
