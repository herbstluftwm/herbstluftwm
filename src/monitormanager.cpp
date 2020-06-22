#include "monitormanager.h"

#include <X11/Xlib.h>
#include <cassert>
#include <memory>

#include "command.h"
#include "completion.h"
#include "ewmh.h"
#include "floating.h"
#include "frametree.h"
#include "globals.h"
#include "ipc-protocol.h"
#include "monitor.h"
#include "monitordetection.h"
#include "panelmanager.h"
#include "rectangle.h"
#include "root.h"
#include "settings.h"
#include "stack.h"
#include "tag.h"
#include "tagmanager.h"
#include "utils.h"
#include "xconnection.h"

using std::endl;
using std::function;
using std::make_pair;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::vector;

MonitorManager* g_monitors;

MonitorManager::MonitorManager()
    : IndexingObject<Monitor>()
    , focus(*this, "focus")
    , by_name_(*this)
    , panels_(nullptr)
    , tags_(nullptr)
    , settings_(nullptr)
{
    cur_monitor = 0;
}

MonitorManager::~MonitorManager() {
    clearChildren();
}

void MonitorManager::injectDependencies(Settings* s, TagManager* t, PanelManager* p) {
    settings_ = s;
    tags_ = t;
    panels_ = p;
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
    m->tag->setVisible(true);
    cur_monitor = 0;

    autoUpdatePads();
    monitor_update_focus_objects();
}

int MonitorManager::indexInDirection(Monitor* relativeTo, Direction dir) {
    RectangleIdxVec rects;
    if (!relativeTo) {
        return -1;
    }
    for (Monitor* mon : *this) {
        rects.push_back(make_pair(mon->index(), mon->rect));
    }
    int result = find_rectangle_in_direction(rects, int(relativeTo->index), dir);
    return result;
}

int MonitorManager::string_to_monitor_index(string str) {
    if (str[0] == '\0') {
        return cur_monitor;
    } else if (str[0] == '-' || str[0] == '+') {
        if (isdigit(str[1])) {
            // relative monitor index
            int idx = cur_monitor + atoi(str.c_str());
            return MOD(idx, size());
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

void MonitorManager::completeMonitorName(Completion& complete) {
    complete.full(""); // the focused monitor
    // complete against relative indices
    complete.full("-1");
    complete.full("+0");
    complete.full("+1");
    for (auto m : *this) {
        // complete against the absolute index
        complete.full(to_string(m->index()));
        // complete against the name
        if (m->name != "") {
            complete.full(m->name);
        }
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
               << ((cur_monitor == i) ? " [FOCUS]" : "")
               << (monitor->lock_tag ? " [LOCKED]" : "")
               << "\n";
        i++;
    }
    return 0;
}

Monitor* MonitorManager::byString(string str) {
    int idx = string_to_monitor_index(str);
    return ((idx >= 0) && idx < static_cast<int>(size())) ? byIdx(idx) : nullptr;
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
                    ": Monitor \"" << monitor_name << "\" not found!\n";
                return HERBST_INVALID_ARGUMENT;
            }
        }
        return cmd(*monitor, Input(input.command(), input.toVector()), output);
    };
}

CommandBinding MonitorManager::byFirstArg(MonitorCommand moncmd, MonitorCompletion moncomplete)
{
    auto cmdBound = byFirstArg(moncmd);
    auto completeBound = [this,moncomplete](Completion& complete) {
        if (complete == 0) {
            this->completeMonitorName(complete);
        } else {
            Monitor* monitor = byString(complete[0]);
            if (!monitor) {
                monitor = focus();
            }
            moncomplete(*monitor, complete);
        }
    };
    return {cmdBound, completeBound};
}

CommandBinding MonitorManager::tagCommand(TagCommand cmd, TagCompletion completer)
{
    auto cmdBound = [this,cmd](Input input, Output output) {
        return cmd(*(this->focus()->tag), input, output);
    };
    auto completeBound =  [this,completer](Completion& complete) {
        completer(*(this->focus()->tag), complete);
    };
    return {cmdBound, completeBound};
}

CommandBinding MonitorManager::tagCommand(function<int (HSTag&)> cmd)
{
    return CommandBinding([this,cmd]() {
        return cmd(*(this->focus()->tag));
    });
}


Monitor* MonitorManager::byTag(HSTag* tag) {
    for (Monitor* m : *this) {
        if (m->tag == tag) {
            return m;
        }
    }
    return nullptr;
}

/**
 * @brief Find the monitor having the given coordinate
 * @param coordinate on a monitor
 * @return The monitor with the coordinate or nullptr if the coordinate is outside of any monitor
 */
Monitor* MonitorManager::byCoordinate(Point2D p)
{
    for (Monitor* m : *this) {
        if (m->rect.x + m->pad_left <= p.x
            && m->rect.x + m->rect.width - m->pad_right > p.x
            && m->rect.y + m->pad_up <= p.y
            && m->rect.y + m->rect.height - m->pad_down > p.y) {
            return &* m;
        }
    }
    return nullptr;
}

Monitor* MonitorManager::byFrame(shared_ptr<Frame> frame)
{
    for (Monitor* m : *this) {
        if (m->tag->frame->contains(frame)) {
            return m;
        }
    }
    return nullptr;
}

void MonitorManager::relayoutTag(HSTag* tag)
{
    Monitor* m = byTag(tag);
    if (m) {
        m->applyLayout();
    }
}

void MonitorManager::relayoutAll()
{
    for (Monitor* m : *this) {
        m->applyLayout();
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
    monitor->tag->setVisible(false);

    monitorStack_.remove(monitor);
    g_monitors->removeIndexed(monitorIdx);

    if (cur_monitor >= static_cast<int>(g_monitors->size())) {
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
        if (!error.empty()) {
            output << input.command() << ": " << error << "\n";
            return HERBST_INVALID_ARGUMENT;
        }
    }
    auto monitor = addMonitor(rect, tag);
    if (!monitorName.empty()) {
        monitor->name = monitorName;
    }

    autoUpdatePads();
    monitor->applyLayout();
    tag->setVisible(true);
    emit_tag_changed(tag, g_monitors->size() - 1);
    dropEnterNotifyEvents.emit();

    return HERBST_EXIT_SUCCESS;
}

string MonitorManager::isValidMonitorName(string name) {
    if (isdigit(name[0])) {
        return "Invalid name \"" + name + "\": The monitor name may not start with a number";
    }
    if (name.empty()) {
        // clearing a name is always OK.
        return "";
    }
    if (find_monitor_by_name(name.c_str())) {
        return "A monitor with the name \"" + name + "\" already exists";
    }
    return "";
}

//! automatically update the pad settings for all monitors
void MonitorManager::autoUpdatePads()
{
    for (Monitor* m : *this) {
        PanelManager::ReservedSpace rs = panels_->computeReservedSpace(m->rect);
        // all the sides in the order as it matters for pad_automatically_set
        vector<pair<Attribute_<int>&, int>> sides = {
            { m->pad_up,    rs.top_     },
            { m->pad_right, rs.right_   },
            { m->pad_down,  rs.bottom_  },
            { m->pad_left,  rs.left_    },
        };
        size_t idx = 0;
        for (auto& it : sides ) {
            if (it.first() != it.second) {
                if (it.second != 0) {
                    // if some panel was added or resized
                    it.first.operator=(it.second);
                    m->pad_automatically_set[idx] = true;
                } else {
                    // if there is no panel, then only clear the pad
                    // if the pad was added by us before
                    if (m->pad_automatically_set[idx]) {
                        it.first.operator=(0);
                        m->pad_automatically_set[idx] = false;
                    }
                }
            }
            idx++;
        }
    }
}

Monitor* MonitorManager::addMonitor(Rectangle rect, HSTag* tag) {
    Monitor* m = new Monitor(settings_, this, rect, tag);
    addIndexed(m);
    monitorStack_.insert(m);
    m->monitorMoved.connect([this]() {
        this->autoUpdatePads();
    });
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
        if (!label_.empty()) {
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
            auto layer = monitor->tag->stack->layers_[layerIdx];

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

/** Add, Move, Remove monitors such that the monitor list matches the given
 * vector of Rectangles
 */
int MonitorManager::setMonitors(const RectangleVec& templates) {
    if (templates.empty()) {
        return HERBST_INVALID_ARGUMENT;
    }
    HSTag* tag = nullptr;
    unsigned i;
    for (i = 0; i < std::min(templates.size(), size()); i++) {
        auto m = byIdx(i);
        if (!m) {
            continue;
        }
        m->rect = templates[i];
    }
    // add additional monitors
    for (; i < templates.size(); i++) {
        tag = tags_->unusedTag();
        if (!tag) {
            return HERBST_TAG_IN_USE;
        }
        addMonitor(templates[i], tag);
        tag->setVisible(true);
    }
    // remove monitors if there are too much
    while (i < size()) {
        removeMonitor(byIdx(i));
    }
    monitor_update_focus_objects();
    autoUpdatePads();
    all_monitors_apply_layout();
    return 0;
}

int MonitorManager::setMonitorsCommand(Input input, Output output) {
    RectangleVec templates;
    string rectangleString;
    while (input >> rectangleString) {
        Rectangle rect = Rectangle::fromStr(rectangleString);
        if (rect.width == 0 || rect.height == 0)
        {
            output << input.command()
                   << ": Rectangle invalid or too small: "
                   << rectangleString << endl;
            return HERBST_INVALID_ARGUMENT;
        }
        templates.push_back(rect);
    }
    if (templates.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }
    int status = setMonitors(templates);

    if (status == HERBST_TAG_IN_USE) {
        output << input.command() << ": There are not enough free tags\n";
    } else if (status == HERBST_INVALID_ARGUMENT) {
        return HERBST_NEED_MORE_ARGS;
    }
    return status;
}

void MonitorManager::setMonitorsCompletion(Completion&) {
    // every parameter can be a rectangle specification.
    // we don't have completion for rectangles
}

void MonitorManager::detectMonitorsCompletion(Completion& complete)
{
    complete.full({"-l", "--list", "--list-all", "--no-disjoin"});
}

int MonitorManager::detectMonitorsCommand(Input input, Output output)
{
    bool list_all = false;
    bool list_only = false;
    bool disjoin = true;
    string arg;
    while (input >> arg) {
        if (arg == "-l" || arg == "--list") {
            list_only = true;
        } else if (arg == "--list-all") {
            list_all = true;
        } else if (arg == "--no-disjoin") {
            disjoin = false;
        } else {
            output << input.command() << ": unknown flag \"" << arg << "\"\n";
            return HERBST_INVALID_ARGUMENT;
        }
    }

    auto root = Root::get();
    if (list_all) {
        for (const auto& detector : MonitorDetection::detectors()) {
            output << detector.name_ << ":";
            if (detector.detect_) {
                for (auto m : detector.detect_(root->X)) {
                    output << " " << m;
                }
            } else {
                output << " disabled";
            }
            output << endl;
        }
        return 0;
    }

    RectangleVec monitor_rects = {};
    for (const auto& detector : MonitorDetection::detectors()) {
        if (detector.detect_) {
            auto rects = detector.detect_(root->X);
            // remove duplicates
            std::sort(rects.begin(), rects.end());
            rects.erase(std::unique(rects.begin(), rects.end()), rects.end());
            // check if this has more outputs than we know already
            if (rects.size() > monitor_rects.size()) {
                monitor_rects = rects;
            }
        }
    }
    if (monitor_rects.empty()) {
        monitor_rects = { root->X.windowSize(root->X.root()) };
    }
    if (list_only) {
        for (auto m : monitor_rects) {
            output << m << "\n";
        }
    } else {
        // possibly disjoin them
        if (disjoin) {
            monitor_rects = disjoin_rects(monitor_rects);
        }
        // apply it
        int ret = g_monitors->setMonitors(monitor_rects);
        if (ret == HERBST_TAG_IN_USE) {
            output << input.command() << ": There are not enough free tags\n";
        }
        return ret;
    }
    return 0;
}

/**
 * @brief Transform a rectangle on the screen into a rectangle relative to one of the monitor.
 * Currently, we pick the monitor that has the biggest intersection with the given rectangle.
 *
 * @param globalGeometry A rectangle whose coordinates are interpreted relative to (0,0) on the screen
 * @return The given rectangle with coordinates relative to a monitor
 */
Rectangle MonitorManager::interpretGlobalGeometry(Rectangle globalGeometry)
{
    int bestArea = 0;
    Monitor* best = nullptr;
    for (Monitor* m : *this) {
        auto intersection = m->rect.intersectionWith(globalGeometry);
        if (!intersection) {
            continue;
        }
        auto area = intersection.width * intersection.height;
        if (area > bestArea) {
            bestArea = area;
            best = m;
        }
    }
    if (best) {
        globalGeometry.x -= best->rect.x + *best->pad_left;
        globalGeometry.y -= best->rect.y + *best->pad_up;
    }
    return globalGeometry;
}

int MonitorManager::raiseMonitorCommand(Input input, Output output) {
    string monitorName = "";
    input >> monitorName;
    Monitor* monitor = string_to_monitor(monitorName.c_str());
    if (!monitor) {
        output << input.command() << ": Monitor \"" << monitorName << "\" not found!\n";
        return HERBST_INVALID_ARGUMENT;
    }
    monitorStack_.raise(monitor);
    restack();
    return 0;
}

void MonitorManager::raiseMonitorCompletion(Completion& complete) {
    if (complete == 0) {
        completeMonitorName(complete);
    } else {
        complete.none();
    }
}

