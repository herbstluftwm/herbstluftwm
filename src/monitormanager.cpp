
#include "monitormanager.h"
#include <memory>

#include "tagmanager.h"
#include "globals.h"
#include "layout.h"
#include "monitor.h"
#include "ipc-protocol.h"

using namespace std;

MonitorManager::MonitorManager(TagManager* tags_)
    : ChildByIndex<HSMonitor>()
    , focus(*this, "focus")
    , by_name(*this)
    , tags(tags_)
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
    HSMonitor* m = add_monitor(rect, &* tag, NULL);
    m->tag->frame->setVisibleRecursive(true);
    g_cur_monitor = 0;

    monitor_update_focus_objects();
}

int monitor_index_in_direction(HSMonitor* m, enum HSDirection dir) {
    int cnt = monitor_count();
    RectangleIdx* rects = g_new0(RectangleIdx, cnt);
    int relidx = -1;
    FOR (i,0,cnt) {
        rects[i].idx = i;
        rects[i].r = monitor_with_index(i)->rect;
        if (monitor_with_index(i) == m) relidx = i;
    }
    HSAssert(relidx >= 0);
    int result = find_rectangle_in_direction(rects, cnt, relidx, dir);
    g_free(rects);
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
            return monitor_index_in_direction(focus(), dir);
        } else {
            return -1;
        }
    } else if (isdigit(string[0])) {
        // absolute monitor index
        int idx = atoi(string.c_str());
        if (idx < 0 || idx >= size()) {
            return -1;
        }
        return idx;
    } else {
        // monitor string
        for (int i = 0; i < size(); i++) {
          if (byIdx(i)->name == string) {
            return i;
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
    return ((idx >= 0) && idx < size()) ? byIdx(idx) : NULL;
}

int MonitorManager::list_padding(Input input, Output output) {
    HSMonitor* monitor;
    input.shift();
    if (input.empty()) {
        monitor = get_current_monitor();
    } else {
        monitor = byString(input.front());
        if (monitor == NULL) {
            output << input.command() << ": Monitor \"" << input.front() << "\" not found!\n";
            return HERBST_INVALID_ARGUMENT;
        }
    }
    output     << monitor->pad_up()
        << " " << monitor->pad_right()
        << " " << monitor->pad_down()
        << " " << monitor->pad_left()
        << "\n";
    return 0;
}


