
#include "monitormanager.h"
#include <memory>
#include <cassert>

#include "tagmanager.h"
#include "globals.h"
#include "layout.h"
#include "monitor.h"
#include "settings.h"
#include "ipc-protocol.h"
#include "utils.h"
#include "stack.h"
#include "ewmh.h"

using namespace std;

MonitorManager::MonitorManager(Settings* settings, TagManager* tags)
    : ChildByIndex<HSMonitor>()
    , focus(*this, "focus")
    , by_name_(*this)
    , tags_(tags)
    , settings_(settings)
{
}

void MonitorManager::clearChildren() {
    ChildByIndex<HSMonitor>::clearChildren();
    focus = {};
    tags_ = {};
}

void MonitorManager::ensure_monitors_are_available() {
    if (size() > 0) {
        // nothing to do
        return;
    }
    // add monitor if necessary
    Rectangle rect = { 0, 0,
            DisplayWidth(g_display, DefaultScreen(g_display)),
            DisplayHeight(g_display, DefaultScreen(g_display))};
    HSTag* tag = tags_->ensure_tags_are_available();
    // add monitor with first tag
    HSMonitor* m = addMonitor(rect, tag);
    m->tag->frame->setVisibleRecursive(true);
    g_cur_monitor = 0;

    monitor_update_focus_objects();
}

int MonitorManager::indexInDirection(HSMonitor* m, enum HSDirection dir) {
    RectangleIdxVec rects;
    int relidx = -1;
    FOR (i,0,size()) {
        rects.push_back(make_pair(i, byIdx(i)->rect));
        if (byIdx(i) == m) relidx = i;
    }
    HSAssert(relidx >= 0);
    int result = find_rectangle_in_direction(rects, relidx, dir);
    return result;
}

int MonitorManager::string_to_monitor_index(std::string string) {
    if (string[0] == '\0') {
        return g_cur_monitor;
    } else if (string[0] == '-' || string[0] == '+') {
        if (isdigit(string[1])) {
            // relative monitor index
            int idx = g_cur_monitor + atoi(string.c_str());
            idx %= size();
            idx += size();
            idx %= size();
            return idx;
        } else if (string[0] == '-') {
            int dir = char_to_direction(string[1]);
            if (dir < 0) return -1;
            return indexInDirection(focus(), (enum HSDirection)dir);
        } else {
            return -1;
        }
    } else if (isdigit(string[0])) {
        // absolute monitor index
        int idx = atoi(string.c_str());
        if (idx < 0 || idx >= (int)size()) {
            return -1;
        }
        return idx;
    } else {
        // monitor string
        for (unsigned i = 0; i < size(); i++) {
          if (byIdx(i)->name == string) {
            return (int)i;
          }
        }
        return -1;
    }
}


int MonitorManager::list_monitors(Input argv, Output output) {
    (void)argv;
    string monitor_name = "";
    int i = 0;
    for (auto monitor : *this) {
        if (monitor->name != "" ) {
            monitor_name = ", named \"" + monitor->name() + "\"";
        } else {
            monitor_name = "";
        }
        output << i << ": " << monitor->rect
               << " with tag \""
               << (monitor->tag ? monitor->tag->name->c_str() : "???")
               << "\""
               << monitor_name
               << (((unsigned int) g_cur_monitor == i) ? " [FOCUS]" : "")
               << (monitor->lock_tag ? " [LOCKED]" : "")
               << "\n";
        i++;
    }
    return 0;
}

HSMonitor* MonitorManager::byString(string str) {
    int idx = string_to_monitor_index(str);
    return ((idx >= 0) && idx < size()) ? byIdx(idx) : nullptr;
}

function<int(Input, Output)> MonitorManager::byFirstArg(HSMonitorCommand cmd)
{
    return [this,cmd](Input input, Output output) -> int {
        HSMonitor *monitor;
        input.shift();
        if (input.empty()) {
            monitor = get_current_monitor();
        } else {
            monitor = byString(input.front());
            if (!monitor) {
                output << input.command() <<
                    ": Monitor \"" << input.front() << "\" not found!\n";
                return HERBST_INVALID_ARGUMENT;
            }
        }
        return cmd(*monitor, input, output);
    };
}

void MonitorManager::relayoutTag(HSTag *tag)
{
    for (HSMonitor* m : *this) {
        if (m->tag == tag) {
            m->applyLayout();
            break;
        }
    }
}

int MonitorManager::removeMonitor(Input input, Output output)
{
    if (input.size() < 2) {
        return HERBST_NEED_MORE_ARGS;
    }

    input.shift();
    string monitorIdxString = input.front();
    auto monitor = byString(monitorIdxString);

    if (monitor == nullptr) {
        output << input.command() << ": Monitor \"" << monitorIdxString << "\" not found!\n";
        return HERBST_INVALID_ARGUMENT;
    }

    if (size() <= 1) {
        output << input.command() << ": Can't remove the last monitor\n";
        return HERBST_FORBIDDEN;
    }

    removeMonitor(monitor);

    return HERBST_EXIT_SUCCESS;
}

void MonitorManager::removeMonitor(HSMonitor* monitor)
{
    auto monitorIdx = index_of(monitor);

    if (g_cur_monitor > index_of(monitor)) {
        // Take into account that the current monitor will have a new
        // index after removal:
        g_cur_monitor--;
    }

    // Hide all clients visible in monitor
    assert(monitor->tag != nullptr);
    assert(monitor->tag->frame != nullptr);
    monitor->tag->frame->setVisibleRecursive(false);

    g_monitors->removeIndexed(monitorIdx);

    delete monitor;

    if (g_cur_monitor >= g_monitors->size()) {
        g_cur_monitor--;
        // if selection has changed, then relayout focused monitor
        get_current_monitor()->applyLayout();
        monitor_update_focus_objects();
        // also announce the new selection
        ewmh_update_current_desktop();
        emit_tag_changed(get_current_monitor()->tag, g_cur_monitor);
    }
    monitor_update_focus_objects();
}


HSMonitor* MonitorManager::addMonitor(Rectangle rect, HSTag* tag) {
    HSMonitor* m = new HSMonitor(settings_, this, rect, tag);
    addIndexed(m);
    return m;
}


int MonitorManager::lock_cmd(Input, Output) {
    lock();
    return 0;
}

void MonitorManager::lock() {
    settings_->monitors_locked = settings_->monitors_locked() + 1;
    lock_number_changed();
}

void MonitorManager::unlock() {
    settings_->monitors_locked = max(0, settings_->monitors_locked() - 1);
    lock_number_changed();
}

int MonitorManager::unlock_cmd(Input, Output) {
    unlock();
    return 0;
}

std::string MonitorManager::lock_number_changed() {
    if (settings_->monitors_locked() < 0) {
        return "must be non-negative";
    }
    if (!settings_->monitors_locked()) {
        // if not locked anymore, then repaint all the dirty monitors
        for (auto m : *this) {
            if (m->dirty) {
                m->applyLayout();
            }
        }
    }
    return {};
}

