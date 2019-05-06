#include "monitormanager.h"

#include <X11/Xlib.h>
#include <cassert>
#include <memory>

#include "ewmh.h"
#include "floating.h"
#include "frametree.h"
#include "globals.h"
#include "ipc-protocol.h"
#include "layout.h"
#include "monitor.h"
#include "settings.h"
#include "stack.h"
#include "tag.h"
#include "tagmanager.h"
#include "utils.h"

using std::function;
using std::make_pair;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;

MonitorManager* g_monitors;

MonitorManager::MonitorManager()
    : IndexingObject<Monitor>()
    , focus(*this, "focus")
    , by_name_(*this)
{
    cur_monitor = 0;
}

MonitorManager::~MonitorManager() {
    clearChildren();
}

void MonitorManager::injectDependencies(Settings* s, TagManager* t) {
    settings_ = s;
    tags_ = t;
}

void MonitorManager::clearChildren() {
    IndexingObject<Monitor>::clearChildren();
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
    Monitor* m = addMonitor(rect, tag);
    m->tag->frame->root_->setVisibleRecursive(true);
    cur_monitor = 0;

    monitor_update_focus_objects();
}

int MonitorManager::indexInDirection(Monitor* m, Direction dir) {
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

int MonitorManager::string_to_monitor_index(string str) {
    if (str[0] == '\0') {
        return cur_monitor;
    } else if (str[0] == '-' || str[0] == '+') {
        if (isdigit(str[1])) {
            // relative monitor index
            int idx = cur_monitor + atoi(str.c_str());
            idx %= size();
            idx += size();
            idx %= size();
            return idx;
        } else if (str[0] == '-') {
            try {
                auto dir = Converter<Direction>::parse(str.substr(1));
                return indexInDirection(focus(), dir);
            } catch (...) {
                return -1;
            }
        } else {
            return -1;
        }
    } else if (isdigit(str[0])) {
        // absolute monitor index
        int idx = atoi(str.c_str());
        if (idx < 0 || idx >= (int)size()) {
            return -1;
        }
        return idx;
    } else {
        // monitor string
        for (unsigned i = 0; i < size(); i++) {
          if (byIdx(i)->name == str) {
            return (int)i;
          }
        }
        return -1;
    }
}


int MonitorManager::list_monitors(Output output) {
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
               << (((unsigned int) cur_monitor == i) ? " [FOCUS]" : "")
               << (monitor->lock_tag ? " [LOCKED]" : "")
               << "\n";
        i++;
    }
    return 0;
}

Monitor* MonitorManager::byString(string str) {
    int idx = string_to_monitor_index(str);
    return ((idx >= 0) && idx < size()) ? byIdx(idx) : nullptr;
}

function<int(Input, Output)> MonitorManager::byFirstArg(MonitorCommand cmd)
{
    return [this,cmd](Input input, Output output) -> int {
        Monitor *monitor;
        string monitor_name;
        if (!(input >> monitor_name)) {
            monitor = get_current_monitor();
        } else {
            monitor = byString(monitor_name);
            if (!monitor) {
                output << input.command() <<
                    ": Monitor \"" << input.front() << "\" not found!\n";
                return HERBST_INVALID_ARGUMENT;
            }
        }
        return cmd(*monitor, Input(input.command(), input.toVector()), output);
    };
}

void MonitorManager::relayoutTag(HSTag *tag)
{
    for (Monitor* m : *this) {
        if (m->tag == tag) {
            m->applyLayout();
            break;
        }
    }
}

int MonitorManager::removeMonitor(Input input, Output output)
{
    string monitorIdxString;
    if (!(input >> monitorIdxString)) {
        return HERBST_NEED_MORE_ARGS;
    }
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

void MonitorManager::removeMonitor(Monitor* monitor)
{
    auto monitorIdx = index_of(monitor);

    if (cur_monitor > index_of(monitor)) {
        // Take into account that the current monitor will have a new
        // index after removal:
        cur_monitor--;
    }

    // Hide all clients visible in monitor
    assert(monitor->tag != nullptr);
    assert(monitor->tag->frame->root_ != nullptr);
    monitor->tag->frame->root_->setVisibleRecursive(false);

    monitorStack_.remove(monitor);
    g_monitors->removeIndexed(monitorIdx);

    if (cur_monitor >= g_monitors->size()) {
        cur_monitor--;
        // if selection has changed, then relayout focused monitor
        get_current_monitor()->applyLayout();
        monitor_update_focus_objects();
        // also announce the new selection
        Ewmh::get().updateCurrentDesktop();
        emit_tag_changed(get_current_monitor()->tag, cur_monitor);
    }
    monitor_update_focus_objects();
}

int MonitorManager::addMonitor(Input input, Output output)
{
    // usage: add_monitor RECTANGLE [TAG [NAME]]
    string rectString, tagName, monitorName;
    input >> rectString;
    if (!input) {
        return HERBST_NEED_MORE_ARGS;
    }
    HSTag* tag = nullptr;
    if (input >> tagName) {
        tag = find_tag(tagName.c_str());
        if (!tag) {
            output << input.command() << ": Tag \"" << tagName << "\" does not exist\n";
            return HERBST_INVALID_ARGUMENT;
        }
        if (find_monitor_with_tag(tag)) {
            output << input.command() <<
                ": Tag \"" << tagName << "\" is already being viewed on a monitor\n";
            return HERBST_TAG_IN_USE;
        }
    } else { // if no tag is supplied
        tag = tags_->unusedTag();
        if (!tag) {
            output << input.command() << ": There are not enough free tags\n";
            return HERBST_TAG_IN_USE;
        }
    }
    // TODO: error message on invalid rectString
    auto rect = Rectangle::fromStr(rectString);
    if (input >> monitorName) {
        string error;
        if (monitorName.empty()) {
            error = "An empty monitor name is not permitted";
        } else {
            error = isValidMonitorName(monitorName);
        }
        if (error != "") {
            output << input.command() << ": " << error << "\n";
            return HERBST_INVALID_ARGUMENT;
        }
    }
    auto monitor = addMonitor(rect, tag);
    if (!monitorName.empty()) {
        monitor->name = monitorName;
    }

    monitor->applyLayout();
    tag->frame->root_->setVisibleRecursive(true);
    emit_tag_changed(tag, g_monitors->size() - 1);
    drop_enternotify_events();

    return HERBST_EXIT_SUCCESS;
}

string MonitorManager::isValidMonitorName(string name) {
    if (isdigit(name[0])) {
        return "Invalid name \"" + name + "\": The monitor name may not start with a number";
    }
    if (find_monitor_by_name(name.c_str())) {
        return "A monitor with the name \"" + name + "\" already exists";
    }
    return "";
}

Monitor* MonitorManager::addMonitor(Rectangle rect, HSTag* tag) {
    Monitor* m = new Monitor(settings_, this, rect, tag);
    addIndexed(m);
    monitorStack_.insert(m);
    return m;
}


void MonitorManager::lock() {
    settings_->monitors_locked = settings_->monitors_locked() + 1;
    lock_number_changed();
}

void MonitorManager::unlock() {
    settings_->monitors_locked = std::max(0, settings_->monitors_locked() - 1);
    lock_number_changed();
}

string MonitorManager::lock_number_changed() {
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

//! return the stack of windows by successive calls to the given yield
//function. The stack is returned from top to bottom, i.e. the topmost element
//is the first element yielded
void MonitorManager::extractWindowStack(bool real_clients, function<void(Window)> yield)
{
    for (Monitor* monitor : monitorStack_) {
        if (!real_clients) {
            yield(monitor->stacking_window);
        }
        monitor->tag->stack->extractWindows(real_clients, yield);
    }
}

//! restack the entire stack including all monitors
void MonitorManager::restack() {
    vector<Window> buf;
    extractWindowStack(false, [&buf](Window w) { buf.push_back(w); });
    XRestackWindows(g_display, buf.data(), buf.size());
    Ewmh::get().updateClientListStacking();
}

class StringTree : public TreeInterface {
public:
    StringTree(string label, vector<shared_ptr<StringTree>> children = {})
        : children_(children)
        , label_(label)
    {};

    size_t childCount() override {
        return children_.size();
    };

    shared_ptr<TreeInterface> nthChild(size_t idx) override {
        return children_.at(idx);
    };

    void appendCaption(Output output) override {
        if (label_ != "") {
            output << " " << label_;
        }
    };

private:
    vector<shared_ptr<StringTree>> children_;
    string label_;
};

int MonitorManager::stackCommand(Output output) {
    vector<shared_ptr<StringTree>> monitors;
    for (Monitor* monitor : monitorStack_) {
        vector<shared_ptr<StringTree>> layers;
        for (size_t layerIdx = 0; layerIdx < LAYER_COUNT; layerIdx++) {
            auto layer = monitor->tag->stack->top[layerIdx];

            vector<shared_ptr<StringTree>> slices;
            for (auto& slice : layer) {
                slices.push_back(make_shared<StringTree>(slice->getLabel()));
            }

            auto layerLabel = g_layer_names[layerIdx];
            layers.push_back(make_shared<StringTree>(layerLabel, slices));
        }

        monitors.push_back(make_shared<StringTree>(monitor->getDescription(), layers));
    }

    auto stackRoot = make_shared<StringTree>("", monitors);
    tree_print_to(stackRoot, output);
    return 0;
}

