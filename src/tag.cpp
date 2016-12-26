/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <sstream>

#include "tag.h"
#include "tagmanager.h"

#include "root.h"
#include "globals.h"
#include "client.h"
#include "clientmanager.h"
#include "ipc-protocol.h"
#include "hook.h"
#include "layout.h"
#include "stack.h"
#include "ewmh.h"
#include "monitor.h"
#include "settings.h"

#include "childbyindex.h"
#include <sstream>

using namespace std;
using namespace herbstluft;

Ptr(TagManager) tags;

static bool    g_tag_flags_dirty = true;
static int* g_raise_on_focus_temporarily;

static int tag_rename(HSTag* tag, char* name, Output output);

void tag_init() {
    g_raise_on_focus_temporarily = &(settings_find("raise_on_focus_temporarily")
                                     ->value.i);
    tags = make_shared<TagManager>();
    herbstluft::Root::get()->addChild(tags);
}

void tag_destroy() {
    tags = Ptr(TagManager)();
}


HSTag::HSTag(std::string name_)
    : index("index", 0)
    , floating("floating", ACCEPT_ALL, false)
    , name("name", AT_THIS(onNameChange), name_)
{
    stack = stack_create();
    frame = make_shared<HSFrameLeaf>(this, shared_ptr<HSFrameSplit>());
    wireAttributes({
        &index,
        &name,
        &floating,
    });
}

HSTag::~HSTag() {
    stack_destroy(this->stack);
}

void HSTag::setIndexAttribute(unsigned long new_index) {
    index = new_index;
}

std::string HSTag::onNameChange() {
    HSTag* found_tag = NULL;
    for (auto t : *tags) {
        if (&* t != this && t->name == *name) {
            found_tag = &* t;
        }
    }
    if (found_tag != NULL) {
        stringstream output;
        output << "Tag \"" << *name << "\" already exists ";
        return output.str();
    } else {
        return {};
    }
}

int    tag_get_count() {
    return tags->size();
}

HSTag* find_tag(const char* name) {
    for (auto t : *tags) {
        if (t->name == name) {
            return &* t;
        }
    }
    return NULL;
}

int tag_index_of(HSTag* tag) {
    return tags->index_of(tag);
}

HSTag* get_tag_by_index(int index) {
    return &* tags->byIdx(index);
}

HSTag* get_tag_by_index_str(char* index_str, bool skip_visible_tags) {
    int index = atoi(index_str);
    // index must be treated relative, if it's first char is + or -
    bool is_relative = array_find("+-", 2, sizeof(char), &index_str[0]) >= 0;
    HSMonitor* monitor = get_current_monitor();
    if (is_relative) {
        int current = tag_index_of(monitor->tag);
        int delta = index;
        index = delta + current;
        // ensure index is valid
        index = MOD(index, tags->size());
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
    return &* tags->byIdx(index);
}

HSTag* find_unused_tag() {
    for (auto t : *tags) {
        if (!find_monitor_with_tag(&* t)) {
            return &* t;
        }
    }
    return NULL;
}

HSTag* add_tag(const char* name) {
    HSTag* find_result = find_tag(name);
    if (find_result) {
        // nothing to do
        return find_result;
    }
    Ptr(HSTag) tag = make_shared<HSTag>(name);
    tags->addIndexed(tag);

    ewmh_update_desktops();
    ewmh_update_desktop_names();
    tag_set_flags_dirty();
    return &* tag;
}

int tag_add_command(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    if (!strcmp("", argv[1])) {
        output << argv[0] << ": An empty tag name is not permitted\n";
        return HERBST_INVALID_ARGUMENT;
    }
    HSTag* tag = add_tag(argv[1]);
    hook_emit_list("tag_added", tag->name->c_str(), NULL);
    return 0;
}

static int tag_rename(HSTag* tag, char* name, Output output) {
    if (find_tag(name)) {
        output << "Error: Tag \"" << name << "\" already exists\n";
        return HERBST_TAG_IN_USE;
    }
    tag->name = name;
    ewmh_update_desktop_names();
    hook_emit_list("tag_renamed", tag->name->c_str(), NULL);
    return 0;
}

int tag_rename_command(int argc, char** argv, Output output) {
    if (argc < 3) {
        return HERBST_NEED_MORE_ARGS;
    }
    if (!strcmp("", argv[2])) {
        output << argv[0] << ": An empty tag name is not permitted\n";
        return HERBST_INVALID_ARGUMENT;
    }
    HSTag* tag = find_tag(argv[1]);
    if (!tag) {
        output << argv[0] << ": Tag \"" << argv[1] << "\" not found\n";
        return HERBST_INVALID_ARGUMENT;
    }
    return tag_rename(tag, argv[2], output);
}

int tag_remove_command(int argc, char** argv, Output output) {
    // usage: remove TAG [TARGET]
    // it removes an TAG and moves all its wins to TARGET
    // if no TARGET is given, current tag is used
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    HSTag* tag = find_tag(argv[1]);
    HSTag* target = (argc >= 3) ? find_tag(argv[2]) : get_current_monitor()->tag;
    if (!tag) {
        output << argv[0] << ": Tag \"" << argv[1] << "\" not found\n";
        return HERBST_INVALID_ARGUMENT;
    } else if (!target) {
        output << argv[0] << ": Tag \"" << argv[2] << "\" not found\n";
    } else if (tag == target) {
        output << argv[0] << ": Cannot merge tag \"" << argv[1] << "\" into itself\n";
        return HERBST_INVALID_ARGUMENT;
    }
    if (find_monitor_with_tag(tag)) {
        output << argv[0] << ": Cannot merge the currently viewed tag\n";
        return HERBST_TAG_IN_USE;
    }
    // prevent dangling tag_previous pointers
    all_monitors_replace_previous_tag(tag, target);
    // save all these windows
    vector<HSClient*> buf;
    tag->frame->foreachClient([](HSClient* client, void* data) {
        vector<HSClient*>* buf = (vector<HSClient*>*) data;
        (*buf).push_back(client);
    }, &buf);
    for (auto client : buf) {
        stack_remove_slice(client->tag()->stack, client->slice);
        client->setTag(target);
        stack_insert_slice(client->tag()->stack, client->slice);
        ewmh_window_update_tag(client->window_, client->tag());
        target->frame->insertClient(client);
    }
    tag->frame = shared_ptr<HSFrame>();
    HSMonitor* monitor_target = find_monitor_with_tag(target);
    if (monitor_target) {
        // if target monitor is viewed, then show windows
        monitor_apply_layout(monitor_target);
        for (auto c: buf) {
            c->set_visible(true);
        }
    }
    // remove tag
    string oldname = tag->name;
    tags->removeIndexed(tags->index_of(tag));
    ewmh_update_current_desktop();
    ewmh_update_desktops();
    ewmh_update_desktop_names();
    tag_set_flags_dirty();
    hook_emit_list("tag_removed", oldname.c_str(), target->name->c_str(), NULL);
    return 0;
}

int tag_set_floating_command(int argc, char** argv, Output output) {
    // usage: floating [[tag] on|off|toggle]
    HSTag* tag = get_current_monitor()->tag;
    const char* action = (argc > 1) ? argv[1] : "toggle";
    if (argc >= 3) {
        // if a tag is specified
        tag = find_tag(argv[1]);
        action = argv[2];
        if (!tag) {
            output << argv[0] << ": Tag \"" << argv[1] << "\" not found\n";
            return HERBST_INVALID_ARGUMENT;
        }
    }

    bool new_value = string_to_bool(action, tag->floating);

    if (!strcmp(action, "status")) {
        // just print status
        output << (tag->floating ? "on" : "off");
    } else {
        // assign new value and rearrange if needed
        tag->floating = new_value;

        HSMonitor* m = find_monitor_with_tag(tag);
        HSDebug("setting tag:%s->floating to %s\n", tag->name->c_str(), tag->floating ? "on" : "off");
        if (m != NULL) {
            monitor_apply_layout(m);
        }
    }
    return 0;
}

void tag_force_update_flags() {
    g_tag_flags_dirty = false;
    // unset all tags
    for (auto t : *tags) {
        t->flags = 0;
    }
    // update flags
    for (auto c : herbstluft::Root::clients()->clients()) {
        auto client = c.second;
        TAG_SET_FLAG(client->tag(), TAG_FLAG_USED);
        if (client->urgent_) {
            TAG_SET_FLAG(client->tag(), TAG_FLAG_URGENT);
        }
    }
}

void tag_update_flags() {
    if (g_tag_flags_dirty) {
        tag_force_update_flags();
    }
}

void tag_set_flags_dirty() {
    g_tag_flags_dirty = true;
    hook_emit_list("tag_flags", NULL);
}

void ensure_tags_are_available() {
    if (tags->size() > 0) {
        // nothing to do
        return;
    }
    add_tag("default");
}

HSTag* find_tag_with_toplevel_frame(HSFrame* frame) {
    for (auto t : *tags) {
        if (&* t->frame == frame) {
            return &* t;
        }
    }
    return NULL;
}

int tag_move_window_command(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    HSTag* target = find_tag(argv[1]);
    if (!target) {
        output << argv[0] << ": Tag \"" << argv[1] << "\" not found\n";
        return HERBST_INVALID_ARGUMENT;
    }
    tag_move_focused_client(target);
    return 0;
}

int tag_move_window_by_index_command(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    bool skip_visible = false;
    if (argc >= 3 && !strcmp(argv[2], "--skip-visible")) {
        skip_visible = true;
    }
    HSTag* tag = get_tag_by_index_str(argv[1], skip_visible);
    if (!tag) {
        output << argv[0] << ": Invalid index \"" << argv[1] << "\"\n";
        return HERBST_INVALID_ARGUMENT;
    }
    tag_move_focused_client(tag);
    return 0;
}

void tag_move_focused_client(HSTag* target) {
    HSClient* client = get_current_monitor()->tag->frame->focusedClient();
    if (client == 0) {
        // nothing to do
        return;
    }
    tag_move_client(client, target);
}

void tag_move_client(HSClient* client, HSTag* target) {
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
    monitor_apply_layout(monitor_source);
    monitor_apply_layout(monitor_target);
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

void tag_update_focus_layer(HSTag* tag) {
    HSClient* focus = tag->frame->focusedClient();
    stack_clear_layer(tag->stack, LAYER_FOCUS);
    if (focus) {
        // enforce raise_on_focus_temporarily if there is at least one
        // fullscreen window or if the tag is in tiling mode
        if (!stack_is_layer_empty(tag->stack, LAYER_FULLSCREEN)
            || *g_raise_on_focus_temporarily
            || focus->tag()->floating == false) {
            stack_slice_add_layer(tag->stack, focus->slice, LAYER_FOCUS);
        }
    }
    HSMonitor* monitor = find_monitor_with_tag(tag);
    if (monitor) {
        monitor_restack(monitor);
    }
}

void tag_foreach(void (*action)(HSTag*,void*), void* data) {
    for (auto tag : *tags) {
        action(&* tag, data);
    }
}

static void tag_update_focus_layer_helper(HSTag* tag, void* data) {
    (void) data;
    tag_update_focus_layer(tag);
}
void tag_update_each_focus_layer() {
    tag_foreach(tag_update_focus_layer_helper, NULL);
}

void tag_update_focus_objects() {
}

