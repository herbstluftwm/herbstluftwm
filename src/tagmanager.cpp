#include "tagmanager.h"

#include <memory>

#include "argparse.h"
#include "client.h"
#include "command.h"
#include "completion.h"
#include "ewmh.h"
#include "globals.h"
#include "ipc-protocol.h"
#include "monitor.h"
#include "monitormanager.h"
#include "utils.h"

using std::function;
using std::string;
using std::shared_ptr;
using std::vector;

TagManager* global_tags;

template<>
RunTimeConverter<HSTag*>* Converter<HSTag*>::converter = nullptr;

TagManager::TagManager()
    : IndexingObject()
    , focus_(*this, "focus")
    , by_name_(*this)
    , settings_(nullptr)
{
    indicesChanged.connect([](){
        Ewmh::get().updateDesktopNames();
        Ewmh::get().updateCurrentDesktop();
        tag_set_flags_dirty();
    });

    setDoc(
        "The tags (or virtual desktops or workspaces). This contains "
        " an entry \'index\' for each tag with the given \'index\'."
    );
    focus_.setDoc(
        "the object of the focused tag, equivalently, "
        "the tag on the focused monitor."
    );
}

void TagManager::injectDependencies(MonitorManager* m, Settings *s) {
    monitors_ = m;
    settings_ = s;
}

HSTag* TagManager::parse(const string& str)
{
    HSTag* t = find(str);
    if (t) {
        return t;
    } else {
        throw std::invalid_argument("no such tag: " + str);
    }
}

string TagManager::str(HSTag* tag)
{
    return tag->name();
}

HSTag* TagManager::find(const string& name) {
    for (auto t : *this) {
        if (t->name == name) {
            return t;
        }
    }
    return {};
}

void TagManager::completeEntries(Completion& complete) {
    for (auto t : *this) {
        complete.full(t->name);
    }
}

//! if the name is a valid tag name, return "", otherwise return an error message
string TagManager::isValidTagName(string name) {
    if (name.empty()) {
        return "An empty tag name is not permitted";
    }
    if (find(name)) {
        return "A tag with the name \"" + name + "\" already exists";
    }
    return "";
}

HSTag* TagManager::add_tag(const string& name) {
    HSTag* find_result = find(name);
    if (find_result) {
        // nothing to do
        return find_result;
    }
    if (name.empty()) {
        // empty name is not allowed
        return nullptr;
    }
    HSTag* tag = new HSTag(name, this, settings_);
    addIndexed(tag);
    tag->name.changed().connect([this,tag]() {
        this->onTagRename(tag);
        tag->oldName_ = tag->name;
    });
    tag->needsRelayout_.connect([this,tag]() { this->needsRelayout_.emit(tag); });

    Ewmh::get().updateDesktops();
    Ewmh::get().updateDesktopNames();
    tag_set_flags_dirty();
    return tag;
}

int TagManager::tag_add_command(Input input, Output output) {
    if (input.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }
    if (input.front().empty()) {
        output << input.command() << ": An empty tag name is not permitted\n";
        return HERBST_INVALID_ARGUMENT;
    }
    HSTag* tag = add_tag(input.front());
    hook_emit({"tag_added", tag->name});
    return 0;
}

void TagManager::mergeTagCommand(CallOrComplete invoc) {
    HSTag* tagToRemove = nullptr;
    HSTag* targetTag = monitors_->focus()->tag;
    ArgParse().mandatory(tagToRemove)
            .optional(targetTag)
            .command(invoc,
                     [&] (Output output) {
        if (!mergeTag(tagToRemove, targetTag)) {
            output << invoc.command() << ": Cannot merge the currently viewed tag\n";
            return HERBST_TAG_IN_USE;
        }
        return HERBST_EXIT_SUCCESS;
    });
}

bool TagManager::mergeTag(HSTag* tagToRemove, HSTag* targetTag)
{
    if (monitors_->byTag(tagToRemove)) {
        return false;
    }
    // Prevent dangling tag_previous pointers
    monitors_->replacePreviousTag(tagToRemove, targetTag);

    // Collect all clients in tag
    vector<Client*> clients;
    tagToRemove->foreachClient([&clients](Client* client) {
        clients.push_back(client);
    });

    // Move clients to target tag
    for (auto client : clients) {
        client->tag()->removeClientSlice(client);
        client->setTag(targetTag);
        client->tag()->insertClientSlice(client);
        targetTag->insertClient(client, {}, false);
    }

    // Ask target tag to make transferred clients visible if necessary
    Monitor* monitor_target = find_monitor_with_tag(targetTag);
    if (monitor_target) {
        monitor_target->applyLayout();
        targetTag->setVisible(true);
    }

    // Remove tag
    string removedName = tagToRemove->name;
    removeIndexed(index_of(tagToRemove));
    Ewmh::get().updateCurrentDesktop();
    Ewmh::get().updateDesktops();
    Ewmh::get().updateDesktopNames();
    tag_set_flags_dirty();
    hook_emit({"tag_removed", removedName, targetTag->name()});
    return true;
}

void TagManager::tag_rename_command(CallOrComplete invoc) {
    HSTag* tag = nullptr;
    string new_name;
    ArgParse().mandatory(tag)
            .mandatory(new_name)
            .command(invoc,
                     [&] (Output output) {
        auto error = tag->name.change(new_name);
        if (!error.empty()) {
            output << invoc.command() << ": " << error << "\n";
            return HERBST_INVALID_ARGUMENT;
        }
        return HERBST_EXIT_SUCCESS;
    });
}

void TagManager::onTagRename(HSTag* tag) {
    Ewmh::get().updateDesktopNames();
    hook_emit({"tag_renamed", tag->oldName_, tag->name()});
}

HSTag* TagManager::ensure_tags_are_available() {
    if (size() > 0) {
        return byIdx(0);
    } else {
        return add_tag("default");
    }
}

HSTag* TagManager::byIndexStr(const string& index_str, bool skip_visible_tags) {
    int index;
    try {
        size_t pos = 0;
        index = stoi(index_str, &pos);
        if (pos != index_str.size()) {
            // index_str has a non-numeric suffix
            return nullptr;
        }
    } catch (...) {
        return nullptr;
    }
    // index must be treated relative, if its first char is + or -
    bool is_relative = index_str[0] == '+' || index_str[0] == '-';
    Monitor* monitor = get_current_monitor();
    if (is_relative) {
        int current = index_of(monitor->tag);
        int delta = index;
        index = delta + current;
        // ensure index is valid
        index = MOD(index, size());
        if (skip_visible_tags) {
            HSTag* tag = byIdx(index);
            for (size_t i = 0; find_monitor_with_tag(tag); i++) {
                if (i >= size()) {
                    // if we tried each tag then there is no invisible tag
                    return nullptr;
                }
                index += delta;
                index = MOD(index, size());
                tag = byIdx(index);
            }
        }
    } else {
        // if it is absolute, then check index
        if (index < 0 || (size_t)index >= size()) {
            HSDebug("invalid tag index %d\n", index);
            return nullptr;
        }
    }
    return byIdx(index);
}

void TagManager::moveFocusedClient(HSTag* target) {
    Client* client = monitors_->focus()->tag->focusedClient();
    if (!client) {
        return;
    }
    moveClient(client, target);
}

void TagManager::moveClient(Client* client, HSTag* target, string frameIndex, bool focus) {
    HSTag* tag_source = client->tag();
    Monitor* monitor_source = find_monitor_with_tag(tag_source);
    if (tag_source == target && frameIndex.empty()) {
        // nothing to do
        return;
    }
    Monitor* monitor_target = find_monitor_with_tag(target);
    tag_source->removeClient(client);
    // insert window into target
    target->insertClient(client, frameIndex, focus);
    if (tag_source != target) {
        client->tag()->removeClientSlice(client);
        client->setTag(target);
        client->tag()->insertClientSlice(client);
    }

    // refresh things, hide things, layout it, and then show it if needed
    if (monitor_source && !monitor_target) {
        // window is moved to invisible tag
        // so hide it
        client->set_visible(false);
    }
    if (monitor_source) {
        monitor_source->applyLayout();
    }
    if (monitor_target) {
        monitor_target->applyLayout();
    }
    if (!monitor_source && monitor_target) {
        client->set_visible(!client->minimized_());
    }
    tag_set_flags_dirty();
}

void TagManager::tag_move_window_command(CallOrComplete invoc) {
    HSTag* target = nullptr;
    ArgParse().mandatory(target)
            .command(invoc,
                     [&] (Output) {
        moveFocusedClient(target);
        return 0;
    });
}

void TagManager::tag_move_window_by_index_command(CallOrComplete invoc) {
    string tagIndex;
    bool skip_visible = false;
    ArgParse().mandatory(tagIndex, {"-1", "+1"})
            .flags({{"--skip-visible", &skip_visible}})
            .command(invoc,
                     [&] (Output output) {
        HSTag* tag = byIndexStr(tagIndex, skip_visible);
        if (!tag) {
            output << invoc.command() << ": Invalid index \"" << tagIndex << "\"\n";
            return HERBST_INVALID_ARGUMENT;
        }
        moveFocusedClient(tag);
        return HERBST_EXIT_SUCCESS;
    });
}

function<int(Input, Output)> TagManager::frameCommand(FrameCommand cmd) {
    return [cmd,this](Input input, Output output) -> int {
        return cmd(*(this->focus_()->frame()), input, output);
    };
}

CommandBinding TagManager::frameCommand(FrameCommand cmd, FrameCompleter completer)
{
    return { frameCommand(cmd), frameCompletion(completer) };
}

CommandBinding TagManager::frameCommand(FrameCallOrComplete cmd)
{
    return CommandBinding([this,cmd](CallOrComplete invoc) {
        ((this->focus_()->frame()) ->* cmd) (invoc);
    });
}

function<int()> TagManager::frameCommand(function<int(FrameTree&)> cmd) {
    return [cmd,this]() -> int {
        return cmd(*(this->focus_()->frame()));
    };
}

function<void(Completion&)> TagManager::frameCompletion(FrameCompleter completer) {
    return [completer,this](Completion& complete) {
        FrameTree* ft = focus_()->frame();
        (ft ->* completer)(complete);
    };
}

HSTag* TagManager::unusedTag() {
    for (auto t : *this) {
        if (!find_monitor_with_tag(&* t)) {
            return t;
        }
    }
    return nullptr;
}

void TagManager::updateFocusObject(Monitor* focusedMonitor) {
    focus_ = focusedMonitor->tag;
}

int TagManager::floatingCmd(Input input, Output output) {
    // usage: floating [[tag] on|off|toggle|status]
    string newValue, tagName;
    if (input.size() >= 2) {
        input >> tagName >> newValue;
    } else if (input.size() == 1) {
        input >> newValue;
    } else {
        tagName = "";
    }
    if (newValue.empty()) {
        newValue = "toggle";
    }
    HSTag* tag = monitors_->focus()->tag;
    if (!tagName.empty()) {
        tag = find(tagName);
        if (!tag) {
            output << input.command() << ": Tag \"" << tagName << "\" not found\n";
            return HERBST_INVALID_ARGUMENT;
        }
    }
    if (newValue == "status") {
        output << (tag->floating ? "on" : "off");
    } else {
        string msg = tag->floating.change(newValue);
        if (!msg.empty()) {
            output << msg << "\n";
            return HERBST_INVALID_ARGUMENT;
        }
    }
    return 0;
}

void TagManager::floatingComplete(Completion &complete)
{
   if (complete == 0) {
       completeEntries(complete);
   }
   if (complete == 0 || complete == 1) {
       if (complete == 1 && !find(complete[0])) {
           // if the first parameter is not a tag, then
           // there can't be a second parameter
           complete.none();
           return;
       }
       complete.full("status");
       // here, we pass a bool-pointer to the completion to get 'toggle' as one of the completion options
       // This is much simpler than passing the actual floating state of the tag
       bool b = true;
       Converter<bool>::complete(complete, &b);
   } else {
       complete.none();
   }
}
