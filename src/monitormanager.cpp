
#include "monitormanager.h"
#include <memory>

#include "tagmanager.h"
#include "globals.h"
#include "layout.h"
#include "monitor.h"
#include "settings.h"
#include "ipc-protocol.h"

using namespace std;

MonitorManager::MonitorManager(Settings* settings_, TagManager* tags_)
    : ChildByIndex<HSMonitor>()
    , focus(*this, "focus")
    , by_name(*this)
    , tags(tags_)
    , settings(settings_)
{
}

void MonitorManager::clearChildren() {
    ChildByIndex<HSMonitor>::clearChildren();
    focus = {};
    tags = {};
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
    HSTag* tag = tags->ensure_tags_are_available();
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
            enum HSDirection dir = char_to_direction(string[1]);
            if (dir < 0) return -1;
            return indexInDirection(focus(), dir);
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

HSMonitor* MonitorManager::addMonitor(Rectangle rect, HSTag* tag) {
    HSMonitor* m = new HSMonitor(settings, this, rect, tag);
    addIndexed(m);
    return m;
}


int MonitorManager::lock_cmd(Input, Output) {
    lock();
    return 0;
}

void MonitorManager::lock() {
    settings->monitors_locked = settings->monitors_locked() + 1;
    lock_number_changed();
}

void MonitorManager::unlock() {
    settings->monitors_locked = max(0, settings->monitors_locked() - 1);
    lock_number_changed();
}

int MonitorManager::unlock_cmd(Input, Output) {
    unlock();
    return 0;
}

std::string MonitorManager::lock_number_changed() {
    if (settings->monitors_locked() < 0) {
        return "must be non-negative";
    }
    if (!settings->monitors_locked()) {
        // if not locked anymore, then repaint all the dirty monitors
        for (auto m : *this) {
            if (m->dirty) {
                m->applyLayout();
            }
        }
    }
    return {};
}

