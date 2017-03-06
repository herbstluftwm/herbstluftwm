#include "tagmanager.h"
#include <memory>

#include "globals.h"
#include "monitor.h"
#include "ipc-protocol.h"
#include "ewmh.h"
#include "monitormanager.h"
#include "client.h"

using namespace std;

Ptr(TagManager) tags;

TagManager::TagManager()
    : ChildByIndex()
    , by_name(*this)
{
}

void TagManager::setMonitorManager(const std::shared_ptr<MonitorManager>& m_) {
    monitors = m_;
}

std::shared_ptr<HSTag> TagManager::find(const std::string& name) {
    for (auto t : *this) {
        if (t->name == name) {
            return t;
        }
    }
    return {};
}

std::shared_ptr<HSTag> TagManager::add_tag(const std::string& name) {
    Ptr(HSTag) find_result = find(name);
    if (find_result) {
        // nothing to do
        return find_result;
    }
    Ptr(HSTag) tag = make_shared<HSTag>(name);
    addIndexed(tag);

    ewmh_update_desktops();
    ewmh_update_desktop_names();
    tag_set_flags_dirty();
    return tag;
}

int TagManager::tag_add_command(Input input, Output output) {
    input.shift();
    if (input.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }
    if ("" == input.front()) {
        output << input.command() << ": An empty tag name is not permitted\n";
        return HERBST_INVALID_ARGUMENT;
    }
    Ptr(HSTag) tag = add_tag(input.front());
    hook_emit_list("tag_added", tag->name->c_str(), NULL);
    return 0;
}

int TagManager::tag_rename_command(Input input, Output output) {
    if (input.size() < 3) {
        return HERBST_NEED_MORE_ARGS;
    }
    input.shift();
    auto old_name = input.front();
    input.shift();
    auto new_name = input.front();
    if (new_name == "") {
        output << input.command() << ": An empty tag name is not permitted\n";
        return HERBST_INVALID_ARGUMENT;
    }
    Ptr(HSTag) tag = find(old_name);
    if (!tag) {
        output << input.command() << ": Tag \"" << old_name << "\" not found\n";
        return HERBST_INVALID_ARGUMENT;
    }
    if (find(new_name)) {
        output << "Error: Tag \"" << new_name << "\" already exists\n";
        return HERBST_TAG_IN_USE;
    }
    tag->name = new_name;
    ewmh_update_desktop_names();
    hook_emit_list("tag_renamed", new_name.c_str(), NULL);
    return 0;
}

std::shared_ptr<HSTag> TagManager::ensure_tags_are_available() {
    if (size() > 0) {
        return byIdx(0);
    } else {
        return add_tag("default");
    }
}

shared_ptr<HSTag> TagManager::byIndexStr(const string& index_str, bool skip_visible_tags) {
    int index = stoi(index_str);
    // index must be treated relative, if it's first char is + or -
    bool is_relative = index_str[0] == '+' || index_str[0] == '-';
    HSMonitor* monitor = get_current_monitor();
    if (is_relative) {
        int current = index_of(monitor->tag);
        int delta = index;
        index = delta + current;
        // ensure index is valid
        index = MOD(index, size());
        if (skip_visible_tags) {
            Ptr(HSTag) tag = tags->byIdx(index);
            for (int i = 0; find_monitor_with_tag(&* tag); i++) {
                if (i >= tags->size()) {
                    // if we tried each tag then there is no invisible tag
                    return NULL;
                }
                index += delta;
                index = MOD(index, tags->size());
                tag = tags->byIdx(index);
            }
        }
    } else {
        // if it is absolute, then check index
        if (index < 0 || index >= tags->size()) {
            HSDebug("invalid tag index %d\n", index);
            return NULL;
        }
    }
    return byIdx(index);
}

