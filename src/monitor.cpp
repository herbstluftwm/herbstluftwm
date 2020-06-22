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
    , rect(rect_)
    , settings(settings_)
    , monman(monman_)
{
    for (auto i : {&pad_up, &pad_left, &pad_right, &pad_down}) {
        i->setWriteable();
        i->changed().connect(this, &Monitor::applyLayout);
    }

    stacking_window = XCreateSimpleWindow(g_display, g_root,
                                             42, 42, 42, 42, 1, 0, 0);

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
    if (isFocused && res.focus) {
        // activate the focus layer if requested by the setting
        // or if there is a fullscreen client potentially covering
        // the focused client.
        // Also activate raise on focus in tiling mode to make the decoration
        // of the focused window look better. If we don't raise it
        // (temporarily), then the shadow of another window can
        // cover the decoration of the focused client. To avoid that
        // the decoration of the focused window is covered by the shadow
        // of an unfocused window,
        // we raise the focused window. Without shadows, this has no effect.
        if (g_settings->raise_on_focus_temporarily()
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

int monitor_rect_command(int argc, char** argv, Output output) {
    // usage: monitor_rect [[-p] INDEX]
    char* monitor_str = nullptr;
    Monitor* m = nullptr;
    bool with_pad = false;

    // if monitor is supplied
    if (argc > 1) {
        monitor_str = argv[1];
    }
    // if -p is supplied
    if (argc > 2) {
        monitor_str = argv[2];
        if (!strcmp("-p", argv[1])) {
            with_pad = true;
        } else {
            output << argv[0] <<
                ": Invalid argument \"" << argv[1] << "\"\n";
            return HERBST_INVALID_ARGUMENT;
        }
    }
    // if an index is set
    if (monitor_str) {
        m = string_to_monitor(monitor_str);
        if (!m) {
            output << argv[0] <<
                ": Monitor \"" << monitor_str << "\" not found!\n";
            return HERBST_INVALID_ARGUMENT;
        }
    } else {
        m = get_current_monitor();
    }
    auto rect = m->rect;
    if (with_pad) {
        rect.x += m->pad_left;
        rect.width -= m->pad_left + m->pad_right;
        rect.y += m->pad_up;
        rect.height -= m->pad_up + m->pad_down;
    }
    output << rect.x << " " << rect.y << " " << rect.width << " " << rect.height;
    return 0;
}

int monitor_set_pad_command(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    Monitor* monitor = string_to_monitor(argv[1]);
    if (!monitor) {
        output << argv[0] <<
            ": Monitor \"" << argv[1] << "\" not found!\n";
        return HERBST_INVALID_ARGUMENT;
    }
    if (argc > 2 && argv[2][0] != '\0') {
        monitor->pad_up = atoi(argv[2]);
    }
    if (argc > 3 && argv[3][0] != '\0') {
        monitor->pad_right = atoi(argv[3]);
    }
    if (argc > 4 && argv[4][0] != '\0') {
        monitor->pad_down = atoi(argv[4]);
    }
    if (argc > 5 && argv[5][0] != '\0') {
        monitor->pad_left = atoi(argv[5]);
    }
    monitor->applyLayout();
    return 0;
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

int monitor_set_tag_command(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    Monitor* monitor = get_current_monitor();
    HSTag*  tag = find_tag(argv[1]);
    if (monitor && tag) {
        int ret = monitor_set_tag(monitor, tag);
        if (ret != 0) {
            output << argv[0] << ": Could not change tag";
            if (monitor->lock_tag) {
                output << " (monitor " << monitor->index() << " is locked)";
            }
            output << "\n";
        }
        return ret;
    } else {
        output << argv[0] <<
            ": Invalid tag \"" << argv[1] << "\"\n";
        return HERBST_INVALID_ARGUMENT;
    }
}

int monitor_set_tag_by_index_command(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    bool skip_visible = false;
    if (argc >= 3 && !strcmp(argv[2], "--skip-visible")) {
        skip_visible = true;
    }
    HSTag* tag = global_tags->byIndexStr(argv[1], skip_visible);
    if (!tag) {
        output << argv[0] <<
            ": Invalid index \"" << argv[1] << "\"\n";
        return HERBST_INVALID_ARGUMENT;
    }
    int ret = monitor_set_tag(get_current_monitor(), &* tag);
    if (ret != 0) {
        output << argv[0] <<
            ": Could not change tag (maybe monitor is locked?)\n";
    }
    return ret;
}

int monitor_set_previous_tag_command(Output output) {
    Monitor* monitor = get_current_monitor();
    HSTag*  tag = monitor->tag_previous;
    if (monitor && tag) {
        int ret = monitor_set_tag(monitor, tag);
        if (ret != 0) {
            output << "use_previous: Could not change tag (maybe monitor is locked?)\n";
        }
        return ret;
    } else {
        output << "use_previous: Invalid monitor or tag\n";
        return HERBST_INVALID_ARGUMENT;
    }
}

int monitor_focus_command(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    int new_selection = g_monitors->string_to_monitor_index(argv[1]);
    if (new_selection < 0) {
        output << argv[0] <<
            ": Monitor \"" << argv[1] << "\" not found!\n";
        return HERBST_INVALID_ARGUMENT;
    }
    // really change selection
    monitor_focus_by_index((unsigned)new_selection);
    return 0;
}

int monitor_cycle_command(int argc, char** argv) {
    int delta = 1;
    auto count = g_monitors->size();
    if (argc >= 2) {
        delta = atoi(argv[1]);
    }
    int new_selection = g_monitors->cur_monitor + delta; // signed for delta calculations
    // really change selection
    monitor_focus_by_index((unsigned)MOD(new_selection, count));
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
            old->mouse.x = rx - old->rect.x;
            old->mouse.y = ry - old->rect.y;
            old->mouse.x = CLAMP(old->mouse.x, 0, old->rect.width-1);
            old->mouse.y = CLAMP(old->mouse.y, 0, old->rect.height-1);
        }
    }
    // restore position of new monitor
    // but only if mouse pointer is not already on new monitor
    int new_x, new_y;
    if ((monitor->rect.x <= rx) && (rx < monitor->rect.x + monitor->rect.width)
        && (monitor->rect.y <= ry) && (ry < monitor->rect.y + monitor->rect.height)) {
        // mouse already is on new monitor
    } else {
        // If the mouse is located in a gap indicated by
        // mouse_recenter_gap at the outer border of the monitor,
        // recenter the mouse.
        if (std::min(monitor->mouse.x, abs(monitor->mouse.x - monitor->rect.width))
                < g_settings->mouse_recenter_gap()
            || std::min(monitor->mouse.y, abs(monitor->mouse.y - monitor->rect.height))
                < g_settings->mouse_recenter_gap()) {
            monitor->mouse.x = monitor->rect.width / 2;
            monitor->mouse.y = monitor->rect.height / 2;
        }
        new_x = monitor->rect.x + monitor->mouse.x;
        new_y = monitor->rect.y + monitor->mouse.y;
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
    return x_root - rect.x - pad_left;
}

int Monitor::relativeY(int y_root) {
    return y_root - rect.y - pad_up;
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

int shift_to_monitor(int argc, char** argv, Output output) {
    if (argc <= 1) {
        return HERBST_NEED_MORE_ARGS;
    }
    char* monitor_str = argv[1];
    Monitor* monitor = string_to_monitor(monitor_str);
    if (!monitor) {
        output << monitor_str << ": Invalid monitor\n";
        return HERBST_INVALID_ARGUMENT;
    }
    global_tags->moveFocusedClient(monitor->tag);
    return 0;
}

void all_monitors_replace_previous_tag(HSTag *old, HSTag *newmon) {
    for (auto m : *g_monitors) {
        if (m->tag_previous == old) {
            m->tag_previous = newmon;
        }
    }
}

Rectangle Monitor::getFloatingArea() {
    auto m = this;
    auto r = m->rect;
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
