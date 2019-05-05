#ifndef __HERBSTLUFT_MONITOR_MANAGER_H_
#define __HERBSTLUFT_MONITOR_MANAGER_H_

#include <functional>
#include <string>

#include "byname.h"
#include "indexingobject.h"
#include "link.h"
#include "monitor.h"
#include "plainstack.h"

extern MonitorManager* g_monitors;

class TagManager;
class HSTag;

typedef std::function<int(Monitor&,Input,Output)> MonitorCommand;

class MonitorManager : public IndexingObject<Monitor> {
public:
    MonitorManager();
    ~MonitorManager();
    void injectDependencies(Settings* s, TagManager* t);

    Link_<Monitor> focus;

    void clearChildren();
    void ensure_monitors_are_available();
    Monitor* byString(std::string str);
    int list_monitors(Output output);
    int list_padding(Input input, Output output);
    int string_to_monitor_index(std::string string);
    Monitor* addMonitor(Rectangle rect, HSTag* tag);
    int addMonitor(Input input, Output output);
    // return a command that interprets the first argument
    // as a monitor description and then calls the given command on this monitor
    std::function<int(Input, Output)> byFirstArg(MonitorCommand cmd);
    // relayout the monitor showing this tag, if there is any
    void relayoutTag(HSTag* tag);
    int removeMonitor(Input input, Output output);
    void removeMonitor(Monitor* monitor);
    // if the name is valid monitor name, return "", otherwise return an error message
    std::string isValidMonitorName(std::string name);

    int indexInDirection(Monitor* m, Direction dir);

    void lock();
    void unlock();
    std::string lock_number_changed();

    void extractWindowStack(bool real_clients, std::function<void(Window)> addToStack);
    void restack();

    int cur_monitor;
    PlainStack<Monitor*> monitorStack_; // TODO: make this private

private:
    ByName by_name_;
    TagManager* tags_;
    Settings* settings_;
};

#endif
