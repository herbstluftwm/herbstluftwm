#include <cassert>
#include <cstring>
#include <cstdio>
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
#include "utils.h"

#include "childbyindex.h"
#include <sstream>

using namespace std;

static bool    g_tag_flags_dirty = true;

void tag_init() {
}

void tag_destroy() {
}


HSTag::HSTag(std::string name_, Settings* settings)
{
    stack = stack_create();
    frame = make_shared<HSFrameLeaf>(this, settings, shared_ptr<HSFrameSplit>());
    wireAttributes({
        &index,
        &name,
        &floating,
    });
    floating.setWriteable();
    name.setValidator([this] (std::string new_name) {
        for (auto t : *global_tags) {
            if (t != this && t->name == new_name) {
                return std::string("Tag \"") + new_name + "\" already exists ";
            }
        }
        return std::string();
    });
    name = name_;
}

HSTag::~HSTag() {
    frame = {};
    stack_destroy(this->stack);
}

void HSTag::setIndexAttribute(unsigned long new_index) {
    index = new_index;
}

int    tag_get_count() {
    return global_tags->size();
}

HSTag* find_tag(const char* name) {
    for (auto t : *global_tags) {
        if (t->name == name) {
            return &* t;
        }
    }
    return nullptr;
}

HSTag* get_tag_by_index(int index) {
    return &* global_tags->byIdx(index);
}

HSTag* find_unused_tag() {
    for (auto t : *global_tags) {
        if (!find_monitor_with_tag(&* t)) {
            return &* t;
        }
    }
    return nullptr;
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
    tag->frame->foreachClient([&buf](HSClient* client) {
        buf.push_back(client);
    });
    for (auto client : buf) {
        client->tag()->stack->remove_slice(client->slice);
        client->setTag(target);
        client->tag()->stack->insert_slice(client->slice);
        ewmh_window_update_tag(client->window_, client->tag());
        target->frame->insertClient(client);
    }
    tag->frame = shared_ptr<HSFrame>();
    HSMonitor* monitor_target = find_monitor_with_tag(target);
    if (monitor_target) {
        // if target monitor is viewed, then show windows
        monitor_target->applyLayout();
        for (auto c: buf) {
            c->set_visible(true);
        }
    }
    // remove tag
    string oldname = tag->name;
    global_tags->removeIndexed(global_tags->index_of(tag));
    ewmh_update_current_desktop();
    ewmh_update_desktops();
    ewmh_update_desktop_names();
    tag_set_flags_dirty();
    hook_emit_list("tag_removed", oldname.c_str(), target->name->c_str(), nullptr);
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

    if (!strcmp(action, "status")) {
        // just print status
        output << (tag->floating ? "on" : "off");
    } else {
        // after deleting this, delete include utils.h line
        bool new_value = string_to_bool(action, tag->floating);

        // assign new value and rearrange if needed
        tag->floating = new_value;

        HSMonitor* m = find_monitor_with_tag(tag);
        HSDebug("setting tag:%s->floating to %s\n", tag->name->c_str(), tag->floating ? "on" : "off");
        if (m) {
            m->applyLayout();
        }
    }
    return 0;
}

void tag_force_update_flags() {
    g_tag_flags_dirty = false;
    // unset all tags
    for (auto t : *global_tags) {
        t->flags = 0;
    }
    // update flags
    for (auto c : Root::get()->clients()->clients()) {
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
    hook_emit_list("tag_flags", nullptr);
}

HSTag* find_tag_with_toplevel_frame(HSFrame* frame) {
    for (auto t : *global_tags) {
        if (&* t->frame == frame) {
            return &* t;
        }
    }
    return nullptr;
}

void tag_update_focus_layer(HSTag* tag) {
    HSClient* focus = tag->frame->focusedClient();
    tag->stack->clear_layer(LAYER_FOCUS);
    if (focus) {
        // enforce raise_on_focus_temporarily if there is at least one
        // fullscreen window or if the tag is in tiling mode
        if (!tag->stack->is_layer_empty(LAYER_FULLSCREEN)
            || g_settings->raise_on_focus_temporarily()
            || focus->tag()->floating == false) {
            tag->stack->slice_add_layer(focus->slice, LAYER_FOCUS);
        }
    }
    HSMonitor* monitor = find_monitor_with_tag(tag);
    if (monitor) {
        monitor->restack();
    }
}

void tag_foreach(void (*action)(HSTag*,void*), void* data) {
    for (auto tag : *global_tags) {
        action(&* tag, data);
    }
}

static void tag_update_focus_layer_helper(HSTag* tag, void* data) {
    (void) data;
    tag_update_focus_layer(tag);
}
void tag_update_each_focus_layer() {
    tag_foreach(tag_update_focus_layer_helper, nullptr);
}

void tag_update_focus_objects() {
}

