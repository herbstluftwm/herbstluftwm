/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "tag.h"

#include "globals.h"
#include "clientlist.h"
#include "ipc-protocol.h"
#include "utils.h"
#include "hook.h"
#include "layout.h"
#include "stack.h"
#include "ewmh.h"
#include "monitor.h"
#include "settings.h"

bool    g_tag_flags_dirty = true;
HSObject* g_tag_object;
HSObject* g_tag_by_name;
int* g_raise_on_focus_temporarily;

static int tag_rename(HSTag* tag, char* name, GString* output);

void tag_init() {
    g_tags = g_array_new(false, false, sizeof(HSTag*));
    g_raise_on_focus_temporarily = &(settings_find("raise_on_focus_temporarily")
                                     ->value.i);
    g_tag_object = hsobject_create_and_link(hsobject_root(), "tags");
    HSAttribute attributes[] = {
        ATTRIBUTE_UINT("count", g_tags->len, ATTR_READ_ONLY),
        ATTRIBUTE_LAST,
    };
    hsobject_set_attributes(g_tag_object, attributes);
    g_tag_by_name = hsobject_create_and_link(g_tag_object, "by-name");
}

static void tag_free(HSTag* tag) {
    if (tag->frame) {
        Window* buf;
        size_t count;
        frame_destroy(tag->frame, &buf, &count);
        if (count) {
            g_free(buf);
        }
    }
    stack_destroy(tag->stack);
    hsobject_unlink_and_destroy(g_tag_by_name, tag->object);
    g_string_free(tag->name, true);
    g_string_free(tag->display_name, true);
    g_free(tag);
}

void tag_destroy() {
    int i;
    for (i = 0; i < g_tags->len; i++) {
        HSTag* tag = g_array_index(g_tags, HSTag*, i);
        frame_show_recursive(tag->frame);
        tag_free(tag);
    }
    g_array_free(g_tags, true);
    hsobject_unlink_and_destroy(g_tag_object, g_tag_by_name);
    hsobject_unlink_and_destroy(hsobject_root(), g_tag_object);
}


HSTag* find_tag(char* name) {
    int i;
    for (i = 0; i < g_tags->len; i++) {
        if (!strcmp(g_array_index(g_tags, HSTag*, i)->name->str, name)) {
            return g_array_index(g_tags, HSTag*, i);
        }
    }
    return NULL;
}

int tag_index_of(HSTag* tag) {
    for (int i = 0; i < g_tags->len; i++) {
        if (g_array_index(g_tags, HSTag*, i) == tag) {
            return i;
        }
    }
    return -1;
}

HSTag* get_tag_by_index(int index) {
    if (index < 0 || index >= g_tags->len) {
        return NULL;
    }
    return g_array_index(g_tags, HSTag*, index);
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
        index = MOD(index, g_tags->len);
        if (skip_visible_tags) {
            HSTag* tag = g_array_index(g_tags, HSTag*, index);
            for (int i = 0; find_monitor_with_tag(tag); i++) {
                if (i >= g_tags->len) {
                    // if we tried each tag then there is no invisible tag
                    return NULL;
                }
                index += delta;
                index = MOD(index, g_tags->len);
                tag = g_array_index(g_tags, HSTag*, index);
            }
        }
    } else {
        // if it is absolute, then check index
        if (index < 0 || index >= g_tags->len) {
            HSDebug("invalid tag index %d\n", index);
            return NULL;
        }
    }
    return g_array_index(g_tags, HSTag*, index);
}

HSTag* find_unused_tag() {
    for (int i = 0; i < g_tags->len; i++) {
        if (!find_monitor_with_tag(g_array_index(g_tags, HSTag*, i))) {
            return g_array_index(g_tags, HSTag*, i);
        }
    }
    return NULL;
}

static GString* tag_attr_floating(HSAttribute* attr) {
    HSTag* tag = container_of(attr->value.b, HSTag, floating);
    HSMonitor* m = find_monitor_with_tag(tag);
    if (m != NULL) {
        monitor_apply_layout(m);
    }
    return NULL;
}

static GString* tag_attr_name(HSAttribute* attr) {
    HSTag* tag = container_of(attr->value.str, HSTag, display_name);
    GString* error = g_string_new("");
    int status = tag_rename(tag, tag->display_name->str, error);
    if (status == 0) {
        g_string_free(error, true);
        return NULL;
    } else {
        return error;
    }
}

static void tag_attr_frame_count(void* data, GString* output) {
    HSTag* tag = (HSTag*) data;
    int i = frame_count_clientframes(tag->frame);
    g_string_append_printf(output, "%d", i);
}

HSTag* add_tag(char* name) {
    HSTag* find_result = find_tag(name);
    if (find_result) {
        // nothing to do
        return find_result;
    }
    HSTag* tag = g_new0(HSTag, 1);
    tag->stack = stack_create();
    tag->frame = frame_create_empty(NULL, tag);
    tag->name = g_string_new(name);
    tag->display_name = g_string_new(name);
    tag->floating = false;
    g_array_append_val(g_tags, tag);

    // create object
    tag->object = hsobject_create_and_link(g_tag_by_name, name);
    tag->object->data = tag;
    HSAttribute attributes[] = {
        ATTRIBUTE_STRING(   "name",         tag->display_name,  tag_attr_name),
        ATTRIBUTE_BOOL(     "floating",     tag->floating,  tag_attr_floating),
        ATTRIBUTE_CUSTOM(   "frame_count",  tag_attr_frame_count,  ATTR_READ_ONLY),
        ATTRIBUTE_LAST,
    };
    hsobject_set_attributes(tag->object, attributes);

    ewmh_update_desktops();
    ewmh_update_desktop_names();
    tag_set_flags_dirty();
    return tag;
}

int tag_add_command(int argc, char** argv, GString* output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    if (!strcmp("", argv[1])) {
        g_string_append_printf(output,
            "%s: An empty tag name is not permitted\n", argv[0]);
        return HERBST_INVALID_ARGUMENT;
    }
    HSTag* tag = add_tag(argv[1]);
    hook_emit_list("tag_added", tag->name->str, NULL);
    return 0;
}

static int tag_rename(HSTag* tag, char* name, GString* output) {
    if (find_tag(name)) {
        g_string_append_printf(output,
            "Error: Tag \"%s\" already exists\n", name);
        return HERBST_TAG_IN_USE;
    }
    hsobject_link_rename(g_tag_by_name, tag->name->str, name);
    g_string_assign(tag->name, name);
    g_string_assign(tag->display_name, name);
    ewmh_update_desktop_names();
    hook_emit_list("tag_renamed", tag->name->str, NULL);
    return 0;
}

int tag_rename_command(int argc, char** argv, GString* output) {
    if (argc < 3) {
        return HERBST_NEED_MORE_ARGS;
    }
    HSTag* tag = find_tag(argv[1]);
    if (!tag) {
        g_string_append_printf(output,
            "%s: Tag \"%s\" not found\n", argv[0], argv[1]);
        return HERBST_INVALID_ARGUMENT;
    }
    return tag_rename(tag, argv[2], output);
}

int tag_remove_command(int argc, char** argv, GString* output) {
    // usage: remove TAG [TARGET]
    // it removes an TAG and moves all its wins to TARGET
    // if no TARGET is given, current tag is used
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    HSTag* tag = find_tag(argv[1]);
    HSTag* target = (argc >= 3) ? find_tag(argv[2]) : get_current_monitor()->tag;
    if (!tag) {
        g_string_append_printf(output,
            "%s: Tag \"%s\" not found\n", argv[0], argv[1]);
        return HERBST_INVALID_ARGUMENT;
    } else if (!target) {
        g_string_append_printf(output,
            "%s: Tag \"%s\" not found\n", argv[0], argv[2]);
    } else if (tag == target) {
        g_string_append_printf(output,
            "%s: Cannot merge tag \"%s\" into itself\n", argv[0], argv[1]);
        return HERBST_INVALID_ARGUMENT;
    }
    if (find_monitor_with_tag(tag)) {
        g_string_append_printf(output,
            "%s: Cannot merge the currently viewed tag\n", argv[0]);
        return HERBST_TAG_IN_USE;
    }
    // prevent dangling tag_previous pointers
    all_monitors_replace_previous_tag(tag, target);
    // save all these windows
    Window* buf;
    size_t count;
    frame_destroy(tag->frame, &buf, &count);
    tag->frame = NULL;
    int i;
    for (i = 0; i < count; i++) {
        HSClient* client = get_client_from_window(buf[i]);
        stack_remove_slice(client->tag->stack, client->slice);
        client->tag = target;
        stack_insert_slice(client->tag->stack, client->slice);
        ewmh_window_update_tag(client->window, client->tag);
        frame_insert_window(target->frame, buf[i]);
    }
    HSMonitor* monitor_target = find_monitor_with_tag(target);
    if (monitor_target) {
        // if target monitor is viewed, then show windows
        monitor_apply_layout(monitor_target);
        for (i = 0; i < count; i++) {
            window_show(buf[i]);
        }
    }
    g_free(buf);
    // remove tag
    char* oldname = g_strdup(tag->name->str);
    tag_free(tag);
    for (i = 0; i < g_tags->len; i++) {
        if (g_array_index(g_tags, HSTag*, i) == tag) {
            g_array_remove_index(g_tags, i);
            break;
        }
    }
    ewmh_update_current_desktop();
    ewmh_update_desktops();
    ewmh_update_desktop_names();
    tag_set_flags_dirty();
    hook_emit_list("tag_removed", oldname, target->name->str, NULL);
    g_free(oldname);
    return 0;
}

int tag_set_floating_command(int argc, char** argv, GString* output) {
    // usage: floating [[tag] on|off|toggle]
    HSTag* tag = get_current_monitor()->tag;
    char* action = (argc > 1) ? argv[1] : "toggle";
    if (argc >= 3) {
        // if a tag is specified
        tag = find_tag(argv[1]);
        action = argv[2];
        if (!tag) {
            g_string_append_printf(output,
                "%s: Tag \"%s\" not found\n", argv[0], argv[1]);
            return HERBST_INVALID_ARGUMENT;
        }
    }

    bool new_value = string_to_bool(action, tag->floating);

    if (!strcmp(action, "status")) {
        // just print status
        g_string_append(output, tag->floating ? "on" : "off");
    } else {
        // assign new value and rearrange if needed
        tag->floating = new_value;

        HSMonitor* m = find_monitor_with_tag(tag);
        HSDebug("setting tag:%s->floating to %s\n", tag->name->str, tag->floating ? "on" : "off");
        if (m != NULL) {
            monitor_apply_layout(m);
        }
    }
    return 0;
}

static void client_update_tag_flags(void* key, void* client_void, void* data) {
    (void) key;
    (void) data;
    HSClient* client = client_void;
    if (client) {
        TAG_SET_FLAG(client->tag, TAG_FLAG_USED);
        if (client->urgent) {
            TAG_SET_FLAG(client->tag, TAG_FLAG_URGENT);
        }
    }
}

void tag_force_update_flags() {
    g_tag_flags_dirty = false;
    // unset all tags
    for (int i = 0; i < g_tags->len; i++) {
        g_array_index(g_tags, HSTag*, i)->flags = 0;
    }
    // update flags
    clientlist_foreach(client_update_tag_flags, NULL);
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
    if (g_tags->len > 0) {
        // nothing to do
        return;
    }
    add_tag("default");
}

HSTag* find_tag_with_toplevel_frame(HSFrame* frame) {
    int i;
    for (i = 0; i < g_tags->len; i++) {
        HSTag* m = g_array_index(g_tags, HSTag*, i);
        if (m->frame == frame) {
            return m;
        }
    }
    return NULL;
}

int tag_move_window_command(int argc, char** argv, GString* output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    HSTag* target = find_tag(argv[1]);
    if (!target) {
        g_string_append_printf(output,
            "%s: Tag \"%s\" not found\n", argv[0], argv[1]);
        return HERBST_INVALID_ARGUMENT;
    }
    tag_move_focused_client(target);
    return 0;
}

int tag_move_window_by_index_command(int argc, char** argv, GString* output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    bool skip_visible = false;
    if (argc >= 3 && !strcmp(argv[2], "--skip-visible")) {
        skip_visible = true;
    }
    HSTag* tag = get_tag_by_index_str(argv[1], skip_visible);
    if (!tag) {
        g_string_append_printf(output,
            "%s: Invalid index \"%s\"\n", argv[0], argv[1]);
        return HERBST_INVALID_ARGUMENT;
    }
    tag_move_focused_client(tag);
    return 0;
}

void tag_move_focused_client(HSTag* target) {
    Window window = frame_focused_window(get_current_monitor()->tag->frame);
    if (window == 0) {
        // nothing to do
        return;
    }
    HSClient* client = get_client_from_window(window);
    assert(client);
    tag_move_client(client, target);
}

void tag_move_client(HSClient* client, HSTag* target) {
    HSTag* tag_source = client->tag;
    HSMonitor* monitor_source = find_monitor_with_tag(tag_source);
    if (tag_source == target) {
        // nothing to do
        return;
    }
    HSMonitor* monitor_target = find_monitor_with_tag(target);
    frame_remove_window(tag_source->frame, client->window);
    // insert window into target
    frame_insert_window(target->frame, client->window);
    // enfoce it to be focused on the target tag
    frame_focus_window(target->frame, client->window);
    stack_remove_slice(client->tag->stack, client->slice);
    client->tag = target;
    stack_insert_slice(client->tag->stack, client->slice);
    ewmh_window_update_tag(client->window, client->tag);

    // refresh things, hide things, layout it, and then show it if needed
    if (monitor_source && !monitor_target) {
        // window is moved to invisible tag
        // so hide it
        window_hide(client->window);
    }
    monitor_apply_layout(monitor_source);
    monitor_apply_layout(monitor_target);
    if (!monitor_source && monitor_target) {
        window_show(client->window);
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
    HSClient* focus = get_client_from_window(frame_focused_window(tag->frame));
    stack_clear_layer(tag->stack, LAYER_FOCUS);
    if (focus) {
        // enforce raise_on_focus_temporarily if there is at least one
        // fullscreen window or if the tag is in tiling mode
        if (!stack_is_layer_empty(tag->stack, LAYER_FULLSCREEN)
            || *g_raise_on_focus_temporarily
            || focus->tag->floating == false) {
            stack_slice_add_layer(tag->stack, focus->slice, LAYER_FOCUS);
        }
    }
    HSMonitor* monitor = find_monitor_with_tag(tag);
    if (monitor) {
        monitor_restack(monitor);
    }
}

void tag_foreach(void (*action)(HSTag*,void*), void* data) {
    for (int i = 0; i < g_tags->len; i++) {
        HSTag* tag = g_array_index(g_tags, HSTag*, i);
        action(tag, data);
    }
}

static void tag_update_focus_layer_helper(HSTag* tag, void* data) {
    (void) data;
    tag_update_focus_layer(tag);
}
void tag_update_each_focus_layer() {
    tag_foreach(tag_update_focus_layer_helper, NULL);
}

