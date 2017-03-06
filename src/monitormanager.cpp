
#include "monitormanager.h"
#include <memory>

#include "tagmanager.h"
#include "globals.h"
#include "layout.h"
#include "monitor.h"

using namespace std;

MonitorManager::MonitorManager(Ptr(TagManager) tags_)
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
    if (monitors->size() > 0) {
        // nothing to do
        return;
    }
    // add monitor if necessary
    Rectangle rect = { 0, 0,
            DisplayWidth(g_display, DefaultScreen(g_display)),
            DisplayHeight(g_display, DefaultScreen(g_display))};
    Ptr(HSTag) tag = tags->ensure_tags_are_available();
    // add monitor with first tag
    HSMonitor* m = add_monitor(rect, &* tag, NULL);
    m->tag->frame->setVisibleRecursive(true);
    g_cur_monitor = 0;

    monitor_update_focus_objects();
}

