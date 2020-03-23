#ifndef __HERBSTLUFT_MONITOR_MANAGER_H_
#define __HERBSTLUFT_MONITOR_MANAGER_H_

#include <functional>
#include <string>

#include "byname.h"
#include "command.h"
#include "indexingobject.h"
#include "link.h"
#include "monitor.h"
#include "plainstack.h"

extern MonitorManager* g_monitors;

class Completion;
class TagManager;
class HSTag;
class HSFrame;

typedef std::function<int(Monitor&,Input,Output)> MonitorCommand;
typedef std::function<void(Monitor&,Completion&)> MonitorCompletion;

class MonitorManager : public IndexingObject<Monitor> {
public:
    MonitorManager();
    ~MonitorManager();
    void injectDependencies(Settings* s, TagManager* t);

    Link_<Monitor> focus;

    void clearChildren();
    void ensure_monitors_are_available();
    Monitor* byString(std::string str);
    Monitor* byTag(HSTag* tag);
    Monitor* byCoordinate(Point2D coordinate);
    Monitor* byFrame(std::shared_ptr<HSFrame> frame);
    int list_monitors(Output output);
    int list_padding(Input input, Output output);
    int string_to_monitor_index(std::string string);
    void completeMonitorName(Completion& complete);
    Monitor* addMonitor(Rectangle rect, HSTag* tag);
    int addMonitor(Input input, Output output);
    // return a command that interprets the first argument
    // as a monitor description and then calls the given command on this monitor
    std::function<int(Input, Output)> byFirstArg(MonitorCommand cmd);
    CommandBinding byFirstArg(MonitorCommand cmd, MonitorCompletion completer);
    // relayout the monitor showing this tag, if there is any
    void relayoutTag(HSTag* tag);
    void relayoutAll();
    int removeMonitor(Input input, Output output);
    void removeMonitor(Monitor* monitor);
    // if the name is valid monitor name, return "", otherwise return an error message
    std::string isValidMonitorName(std::string name);

    int indexInDirection(Monitor* m, Direction dir);

    void lock();
    void unlock();
    std::string lock_number_changed();

    int stackCommand(Output output);
    void extractWindowStack(bool real_clients, std::function<void(Window)> addToStack);
    void restack();
    int raiseMonitorCommand(Input input, Output output);
    void raiseMonitorCompletion(Completion& complete);

    int setMonitors(const RectangleVec& templates);
    int setMonitorsCommand(Input input, Output output);
    void setMonitorsCompletion(Completion& complete);

    Rectangle interpretGlobalGeometry(Rectangle globalGeometry);

    int cur_monitor;

private:
    PlainStack<Monitor*> monitorStack_;

    ByName by_name_;
    TagManager* tags_;
    Settings* settings_;
};

#endif
