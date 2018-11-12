#include "tagmanager.h"
#include <memory>

#include "globals.h"
#include "monitor.h"
#include "ipc-protocol.h"
#include "ewmh.h"
#include "monitormanager.h"
#include "client.h"
#include "layout.h"
#include "stack.h"
#include "settings.h"
#include "utils.h"

using namespace std;

TagManager* global_tags;

TagManager::TagManager(Settings* settings)
    : ChildByIndex()
    , by_name_(*this)
    , settings_(settings)
{
}

void TagManager::setMonitorManager(MonitorManager* m_) {
    monitors_ = m_;
}

HSTag* TagManager::find(const std::string& name) {
    for (auto t : *this) {
        if (t->name == name) {
            return t;
        }
    }
    return {};
}

HSTag* TagManager::add_tag(const std::string& name) {
    HSTag* find_result = find(name);
    if (find_result) {
        // nothing to do
        return find_result;
    }
    HSTag* tag = new HSTag(name, settings_);
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
    HSTag* tag = add_tag(input.front());
    hook_emit_list("tag_added", tag->name->c_str(), nullptr);
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
    HSTag* tag = find(old_name);
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
    hook_emit_list("tag_renamed", new_name.c_str(), nullptr);
    return 0;
}

HSTag* TagManager::ensure_tags_are_available() {
    if (size() > 0) {
        return byIdx(0);
    } else {
        return add_tag("default");
    }
}

HSTag* TagManager::byIndexStr(const string& index_str, bool skip_visible_tags) {
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
            HSTag* tag = global_tags->byIdx(index);
            for (int i = 0; find_monitor_with_tag(&* tag); i++) {
                if (i >= global_tags->size()) {
                    // if we tried each tag then there is no invisible tag
                    return nullptr;
                }
                index += delta;
                index = MOD(index, global_tags->size());
                tag = global_tags->byIdx(index);
            }
        }
    } else {
        // if it is absolute, then check index
        if (index < 0 || index >= global_tags->size()) {
            HSDebug("invalid tag index %d\n", index);
            return nullptr;
        }
    }
    return byIdx(index);
}

void TagManager::moveFocusedClient(HSTag* target) {
    HSClient* client = monitors_->focus()->tag->frame->focusedClient();
    if (!client) {
        return;
    }
    moveClient(client, target);
}

void TagManager::moveClient(HSClient* client, HSTag* target) {
    HSTag* tag_source = client->tag();
    HSMonitor* monitor_source = find_monitor_with_tag(tag_source);
    if (tag_source == target) {
        // nothing to do
        return;
    }
    HSMonitor* monitor_target = find_monitor_with_tag(target);
    tag_source->frame->removeClient(client);
    // insert window into target
    target->frame->insertClient(client);
    // enfoce it to be focused on the target tag
    target->frame->focusClient(client);
    stack_remove_slice(client->tag()->stack, client->slice);
    client->setTag(target);
    stack_insert_slice(client->tag()->stack, client->slice);
    ewmh_window_update_tag(client->window_, client->tag());

    // refresh things, hide things, layout it, and then show it if needed
    if (monitor_source && !monitor_target) {
        // window is moved to invisible tag
        // so hide it
        client->set_visible(false);
    }
    if (monitor_source) monitor_source->applyLayout();
    if (monitor_target) monitor_target->applyLayout();
    if (!monitor_source && monitor_target) {
        client->set_visible(true);
    }
    if (monitor_target == get_current_monitor()) {
        frame_focus_recursive(monitor_target->tag->frame);
    }
    else if (monitor_source == get_current_monitor()) {
        frame_focus_recursive(monitor_source->tag->frame);
    }
    tag_set_flags_dirty();
}

int TagManager::tag_move_window_command(Input argv, Output output) {
    if (argv.size() < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    HSTag* target = find(argv[1]);
    if (!target) {
        output << argv[0] << ": Tag \"" << argv[1] << "\" not found\n";
        return HERBST_INVALID_ARGUMENT;
    }
    moveFocusedClient(target);
    return 0;
}

int TagManager::tag_move_window_by_index_command(Input argv, Output output) {
    if (argv.size() < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    bool skip_visible = false;
    if (argv.size() >= 3 && argv[2] == "--skip-visible") {
        skip_visible = true;
    }
    HSTag* tag = global_tags->byIndexStr(argv[1], skip_visible);
    if (!tag) {
        output << argv[0] << ": Invalid index \"" << argv[1] << "\"\n";
        return HERBST_INVALID_ARGUMENT;
    }
    moveFocusedClient(tag);
    return 0;
}

