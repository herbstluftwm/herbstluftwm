#ifndef __HERBSTLUFT_MONITOR_MANAGER_H_
#define __HERBSTLUFT_MONITOR_MANAGER_H_

#include "monitor.h"
#include "childbyindex.h"
#include "byname.h"
#include "child.h"
#include "floating.h" // for find_rectangle_in_direction(), maybe move?

#include <string>
#include <functional>

extern MonitorManager* g_monitors;

class TagManager;
class HSTag;

typedef std::function<int(HSMonitor&,Input,Output)> HSMonitorCommand;

class MonitorManager : public ChildByIndex<HSMonitor> {
public:
    MonitorManager(Settings* settings, TagManager* tags);
    ~MonitorManager();
    Child_<HSMonitor> focus;

    void clearChildren();
    void ensure_monitors_are_available();
    HSMonitor* byString(std::string str);
    int list_monitors(Output output);
    int list_padding(Input input, Output output);
    int string_to_monitor_index(std::string string);
    HSMonitor* addMonitor(Rectangle rect, HSTag* tag);
    int addMonitor(Input input, Output output);
    // return a command that interprets the first argument
    // as a monitor description and then calls the given command on this monitor
    std::function<int(Input, Output)> byFirstArg(HSMonitorCommand cmd);
    // relayout the monitor showing this tag, if there is any
    void relayoutTag(HSTag* tag);
    int removeMonitor(Input input, Output output);
    void removeMonitor(HSMonitor* monitor);
    // if the name is valid monitor name, return "", otherwise return an error message
    std::string isValidMonitorName(std::string name);

    int indexInDirection(HSMonitor* m, Direction dir);

    void lock();
    void unlock();
    std::string lock_number_changed();

    int cur_monitor;
    Stack* monitor_stack;

private:
    ByName by_name_;
    TagManager* tags_;
    Settings* settings_;
};

#endif
