#ifndef __HERBSTLUFT_MONITOR_MANAGER_H_
#define __HERBSTLUFT_MONITOR_MANAGER_H_

#include <functional>
#include <string>

#include "byname.h"
#include "commandio.h"
#include "indexingobject.h"
#include "link.h"
#include "monitor.h"
#include "plainstack.h"
#include "signal.h"

extern MonitorManager* g_monitors;

class CommandBinding;
class Completion;
class PanelManager;
class TagManager;
class HSTag;
class Frame;

typedef std::function<int(Monitor&,Input,Output)> MonitorCommand;
typedef std::function<void(Monitor&,Completion&)> MonitorCompletion;
typedef std::function<int(HSTag&,Input,Output)> TagCommand;
typedef std::function<void(HSTag&,Completion&)> TagCompletion;

class MonitorManager : public IndexingObject<Monitor> {
public:
    MonitorManager();
    ~MonitorManager();
    void injectDependencies(Settings* s, TagManager* t, PanelManager* panels);

    Link_<Monitor> focus;

    void clearChildren();
    void ensure_monitors_are_available();
    Monitor* byString(std::string str);
    Monitor* byTag(HSTag* tag);
    Monitor* byCoordinate(Point2D p);
    Monitor* byFrame(std::shared_ptr<Frame> frame);
    int list_monitors(Output output);
    int list_padding(Input input, Output output);
    int string_to_monitor_index(std::string string);
    void completeMonitorName(Completion& complete);
    Monitor* addMonitor(Rectangle rect, HSTag* tag);
    int addMonitor(Input input, Output output);
    // return a command that interprets the first argument
    // as a monitor description and then calls the given command on this monitor
    CommandBinding byFirstArg(MonitorCommand cmd, MonitorCompletion moncomplete);
    //! run the command on the currently focused tag
    CommandBinding tagCommand(TagCommand cmd, TagCompletion completer);
    CommandBinding tagCommand(std::function<int(HSTag&)> cmd);
    // relayout the monitor showing this tag, if there is any
    void relayoutTag(HSTag* tag);
    void relayoutAll();
    int removeMonitor(Input input, Output output);
    void removeMonitor(Monitor* monitor);
    // if the name is valid monitor name, return "", otherwise return an error message
    std::string isValidMonitorName(std::string name);

    void autoUpdatePads();

    int indexInDirection(Monitor* relativeTo, Direction dir);

    void lock();
    void unlock();
    std::string lock_number_changed();

    int stackCommand(Output output);
    void extractWindowStack(bool real_clients, std::function<void(Window)> yield);
    void restack();
    int raiseMonitorCommand(Input input, Output output);
    void raiseMonitorCompletion(Completion& complete);

    int setMonitors(const RectangleVec& templates);
    int setMonitorsCommand(Input input, Output output);
    void setMonitorsCompletion(Completion& complete);

    int detectMonitorsCommand(Input input, Output output);
    void detectMonitorsCompletion(Completion& complete);

    Rectangle interpretGlobalGeometry(Rectangle globalGeometry);

    int cur_monitor;

    /**
     * @brief this signal is emitted whenever the mouse cursor have
     * entered another window although the user has not moved the cursor
     * manually. In this case the respective events have to be ignored.
     */
    Signal dropEnterNotifyEvents;

private:
    std::function<int(Input, Output)> byFirstArg(MonitorCommand cmd);

    PlainStack<Monitor*> monitorStack_;

    ByName by_name_;
    PanelManager* panels_;
    TagManager* tags_;
    Settings* settings_;
};

#endif
