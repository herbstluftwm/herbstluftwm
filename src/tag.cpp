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
    : frame_count("frame_count", std::bind(&HSTag::computeFrameCount, this))
    , client_count("client_count", std::bind(&HSTag::computeClientCount, this))
    , curframe_windex("curframe_windex",
        [this] () { return frame->getFocusedFrame()->getSelection(); } )
    , curframe_wcount("curframe_wcount",
        [this] () { return frame->getFocusedFrame()->clientCount(); } )
{
    stack = make_shared<HSStack>();
    frame = make_shared<HSFrameLeaf>(this, settings, shared_ptr<HSFrameSplit>());
    wireAttributes({
        &index,
        &name,
        &floating,
        &frame_count,
        &client_count,
        &curframe_windex,
        &curframe_wcount,
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
}

void HSTag::setIndexAttribute(unsigned long new_index) {
    index = new_index;
}

int HSTag::computeFrameCount() {
    int count = 0;
    frame->fmap([](HSFrameSplit*) {},
                [&count](HSFrameLeaf*) { count++; },
                0);
    return count;
}

int HSTag::computeClientCount() {
    int count = 0;
    frame->fmap([](HSFrameSplit*) {},
                [&count](HSFrameLeaf* l) { count += l->clientCount(); },
                0);
    return count;
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

