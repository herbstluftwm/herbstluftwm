/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_MONITOR_MANAGER_H_
#define __HERBSTLUFT_MONITOR_MANAGER_H_

#include "monitor.h"
#include "childbyindex.h"
#include "byname.h"
#include "child.h"
#include <string>
#include <functional>

class TagManager;

typedef int (HSMonitor::*HSMonitorCommand)(Input,Output);

class MonitorManager : public ChildByIndex<HSMonitor> {
public:
    MonitorManager(Settings* settings, TagManager* tags);
    Child_<HSMonitor> focus;

    void clearChildren();
    void ensure_monitors_are_available();
    HSMonitor* byString(std::string str);
    int list_monitors(Input argv, Output output);
    int list_padding(Input input, Output output);
    int string_to_monitor_index(std::string string);
    HSMonitor* addMonitor(Rectangle rect, HSTag* tag);
    // return a command that interprets the first argument
    // as a monitor description and then calls the given command on this monitor
    std::function<int(Input, Output)> byFirstArg(HSMonitorCommand cmd);

    int indexInDirection(HSMonitor* m, enum HSDirection dir);

    void lock();
    void unlock();
    int lock_cmd(Input, Output);
    int unlock_cmd(Input, Output);
    std::string lock_number_changed();
private:
    ByName by_name;
    TagManager* tags;
    Settings* settings;
};

#endif
