#include "tagmanager.h"

#include <memory>

#include "client.h"
#include "ewmh.h"
#include "frametree.h"
#include "globals.h"
#include "ipc-protocol.h"
#include "layout.h"
#include "monitor.h"
#include "monitormanager.h"
#include "stack.h"
#include "utils.h"

using std::function;
using std::string;
using std::vector;

TagManager* global_tags;

TagManager::TagManager()
    : ChildByIndex()
    , by_name_(*this)
{
}

void TagManager::injectDependencies(MonitorManager* m, Settings *s) {
    monitors_ = m;
    settings_ = s;
}

HSTag* TagManager::find(const string& name) {
    for (auto t : *this) {
        if (t->name == name) {
            return t;
        }
    }
    return {};
}

HSTag* TagManager::add_tag(const string& name) {
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
    if (input.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }
    if (input.front().empty()) {
        output << input.command() << ": An empty tag name is not permitted\n";
        return HERBST_INVALID_ARGUMENT;
    }
    HSTag* tag = add_tag(input.front());
    hook_emit_list("tag_added", tag->name->c_str(), nullptr);
    return 0;
}

int TagManager::removeTag(Input input, Output output) {
    string tagNameToRemove;
    if (!(input >> tagNameToRemove)) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto targetTagName = input.empty() ? get_current_monitor()->tag->name : input.front();

    auto targetTag = find(targetTagName);
    auto tagToRemove = find(tagNameToRemove);

    if (!tagToRemove) {
        output << input.command() << ": Tag \"" << tagNameToRemove << "\" not found\n";
        return HERBST_INVALID_ARGUMENT;
    }

    if (!targetTag) {
        output << input.command() << ": Tag \"" << targetTagName << "\" not found\n";
        return HERBST_INVALID_ARGUMENT;
    }

    if (find_monitor_with_tag(tagToRemove)) {
        output << input.command() << ": Cannot merge the currently viewed tag\n";
        return HERBST_TAG_IN_USE;
    }

    // Prevent dangling tag_previous pointers
    all_monitors_replace_previous_tag(tagToRemove, targetTag);

    // Collect all clients in tag
    vector<Client*> clients;
    tagToRemove->frame->root_->foreachClient([&clients](Client* client) {
        clients.push_back(client);
    });

    // Move clients to target tag
    for (auto client : clients) {
        client->tag()->stack->removeSlice(client->slice);
        client->setTag(targetTag);
        client->tag()->stack->insertSlice(client->slice);
        ewmh_window_update_tag(client->window_, client->tag());
        targetTag->frame->focusedFrame()->insertClient(client);
    }

    // Make transferred clients visible if target tag is visible
    Monitor* monitor_target = find_monitor_with_tag(targetTag);
    if (monitor_target) {
        monitor_target->applyLayout();
        for (auto c : clients) {
            c->set_visible(true);
        }
    }

    // Remove tag
    string removedName = tagToRemove->name;
    removeIndexed(index_of(tagToRemove));
    ewmh_update_current_desktop();
    ewmh_update_desktops();
    ewmh_update_desktop_names();
    tag_set_flags_dirty();
    hook_emit_list("tag_removed", removedName.c_str(), targetTag->name->c_str(), nullptr);

    return HERBST_EXIT_SUCCESS;
}

int TagManager::tag_rename_command(Input input, Output output) {
    string old_name, new_name;
    if (!(input >> old_name >> new_name)) {
        return HERBST_NEED_MORE_ARGS;
    }
    if (new_name.empty()) {
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
    Monitor* monitor = get_current_monitor();
    if (is_relative) {
        int current = index_of(monitor->tag);
        int delta = index;
        index = delta + current;
        // ensure index is valid
        index = MOD(index, size());
        if (skip_visible_tags) {
            HSTag* tag = global_tags->byIdx(index);
            for (size_t i = 0; find_monitor_with_tag(tag); i++) {
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
        if (index < 0 || (size_t)index >= global_tags->size()) {
            HSDebug("invalid tag index %d\n", index);
            return nullptr;
        }
    }
    return byIdx(index);
}

void TagManager::moveFocusedClient(HSTag* target) {
    Client* client = monitors_->focus()->tag->frame->root_->focusedClient();
    if (!client) {
        return;
    }
    moveClient(client, target);
}

void TagManager::moveClient(Client* client, HSTag* target) {
    HSTag* tag_source = client->tag();
    Monitor* monitor_source = find_monitor_with_tag(tag_source);
    if (tag_source == target) {
        // nothing to do
        return;
    }
    Monitor* monitor_target = find_monitor_with_tag(target);
    tag_source->frame->root_->removeClient(client);
    // insert window into target
    target->frame->focusedFrame()->insertClient(client);
    // enfoce it to be focused on the target tag
    target->frame->focusClient(client);
    client->tag()->stack->removeSlice(client->slice);
    client->setTag(target);
    client->tag()->stack->insertSlice(client->slice);
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
        frame_focus_recursive(monitor_target->tag->frame->root_);
    }
    else if (monitor_source == get_current_monitor()) {
        frame_focus_recursive(monitor_source->tag->frame->root_);
    }
    tag_set_flags_dirty();
}

int TagManager::tag_move_window_command(Input argv, Output output) {
    if (argv.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }
    HSTag* target = find(argv.front());
    if (!target) {
        output << argv.command() << ": Tag \"" << argv.front() << "\" not found\n";
        return HERBST_INVALID_ARGUMENT;
    }
    moveFocusedClient(target);
    return 0;
}

int TagManager::tag_move_window_by_index_command(Input argv, Output output) {
    if (argv.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto tagIndex = argv.front();
    argv.shift();
    bool skip_visible = (!argv.empty() && argv.front() == "--skip-visible");

    HSTag* tag = global_tags->byIndexStr(tagIndex, skip_visible);
    if (!tag) {
        output << argv.command() << ": Invalid index \"" << tagIndex << "\"\n";
        return HERBST_INVALID_ARGUMENT;
    }
    moveFocusedClient(tag);
    return 0;
}

function<int(Input, Output)> TagManager::frameCommand(FrameCommand cmd) {
    return [cmd](Input input, Output output) -> int {
        // TODO: use this->focus->frame as soon as we have it.
        return cmd(*(get_current_monitor()->tag->frame), input, output);
    };
}
function<int()> TagManager::frameCommand(function<int(FrameTree&)> cmd) {
    return [cmd]() -> int {
        // TODO: use this->focus->frame as soon as we have it.
        return cmd(*(get_current_monitor()->tag->frame));
    };
}


