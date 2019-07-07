#include "tag.h"

#include <cstring>

#include "client.h"
#include "frametree.h"
#include "globals.h"
#include "hlwmcommon.h"
#include "hook.h"
#include "ipc-protocol.h"
#include "layout.h"
#include "monitor.h"
#include "root.h"
#include "settings.h"
#include "stack.h"
#include "tagmanager.h"
#include "types.h"

using std::make_shared;
using std::shared_ptr;
using std::string;

static bool    g_tag_flags_dirty = true;

HSTag::HSTag(string name_, TagManager* tags, Settings* settings)
    : index(this, "index", 0)
    , floating(this, "floating", false, [](bool){return "";})
    , name(this, "name", name_,
        [tags](string newName) { return tags->isValidTagName(newName); })
    , frame_count(this, "frame_count", &HSTag::computeFrameCount)
    , client_count(this, "client_count", &HSTag::computeClientCount)
    , curframe_windex(this, "curframe_windex",
        [this] () { return frame->focusedFrame()->getSelection(); } )
    , curframe_wcount(this, "curframe_wcount",
        [this] () { return frame->focusedFrame()->clientCount(); } )
{
    stack = make_shared<Stack>();
    frame = make_shared<FrameTree>(this, settings);
}

HSTag::~HSTag() {
    frame = {};
}

void HSTag::setIndexAttribute(unsigned long new_index) {
    index = new_index;
}

int HSTag::computeFrameCount() {
    int count = 0;
    frame->root_->fmap([](HSFrameSplit*) {},
                [&count](HSFrameLeaf*) { count++; },
                0);
    return count;
}

int HSTag::computeClientCount() {
    int count = 0;
    frame->root_->fmap([](HSFrameSplit*) {},
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
        bool new_value = Converter<bool>::parse(action, tag->floating);

        // assign new value and rearrange if needed
        tag->floating = new_value;

        Monitor* m = find_monitor_with_tag(tag);
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
    for (auto c : Root::common().clients()) {
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
    hook_emit({"tag_flags"});
}

HSTag* find_tag_with_toplevel_frame(HSFrame* frame) {
    for (auto t : *global_tags) {
        if (&* t->frame->root_ == frame) {
            return &* t;
        }
    }
    return nullptr;
}

