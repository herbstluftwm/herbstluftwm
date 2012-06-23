/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
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

void tag_init() {
    g_tags = g_array_new(false, false, sizeof(HSTag*));
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
    g_string_free(tag->name, true);
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
    tag->floating = false;
    g_array_append_val(g_tags, tag);
    ewmh_update_desktops();
    ewmh_update_desktop_names();
    tag_set_flags_dirty();
    return tag;
}

int tag_add_command(int argc, char** argv) {
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }
    if (!strcmp("", argv[1])) {
        HSDebug("A empty tag name is not permitted\n");
        return HERBST_INVALID_ARGUMENT;
    }
    HSTag* tag = add_tag(argv[1]);
    hook_emit_list("tag_added", tag->name->str, NULL);
    return 0;
}

int tag_rename_command(int argc, char** argv) {
    if (argc < 3) {
        return HERBST_INVALID_ARGUMENT;
    }
    HSTag* tag = find_tag(argv[1]);
    if (!tag) {
        return HERBST_INVALID_ARGUMENT;
    }
    if (find_tag(argv[2])) {
        return HERBST_TAG_IN_USE;
    }
    tag->name = g_string_assign(tag->name, argv[2]);
    ewmh_update_desktop_names();
    hook_emit_list("tag_renamed", tag->name->str, NULL);
    return 0;
}

int tag_remove_command(int argc, char** argv) {
    // usage: remove TAG [TARGET]
    // it removes an TAG and moves all its wins to TARGET
    // if no TARGET is given, current tag is used
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }
    HSTag* tag = find_tag(argv[1]);
    HSTag* target = (argc >= 3) ? find_tag(argv[2]) : get_current_monitor()->tag;
    if (!tag || !target || (tag == target)) {
        return HERBST_INVALID_ARGUMENT;
    }
    HSMonitor* monitor = find_monitor_with_tag(tag);
    HSMonitor* monitor_target = find_monitor_with_tag(target);
    if (monitor) {
        return HERBST_TAG_IN_USE;
    }
    // save all these windows
    Window* buf;
    size_t count;
    frame_destroy(tag->frame, &buf, &count);
    tag->frame = NULL;
    int i;
    for (i = 0; i < count; i++) {
        HSClient* client = get_client_from_window(buf[i]);
        client->tag = target;
        ewmh_window_update_tag(client->window, client->tag);
        frame_insert_window(target->frame, buf[i]);
    }
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

int tag_set_floating_command(int argc, char** argv, GString** result) {
    // usage: floating [[tag] on|off|toggle]
    HSTag* tag = get_current_monitor()->tag;
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }
    char* action = argv[1];
    if (argc >= 3) {
        // if a tag is specified
        tag = find_tag(argv[1]);
        action = argv[2];
        if (!tag) {
            return HERBST_INVALID_ARGUMENT;
        }
    }

    bool new_value = false;
    if (!strcmp(action, "toggle"))      new_value = ! tag->floating;
    else if (!strcmp(action, "on"))     new_value = true;
    else if (!strcmp(action, "off"))    new_value = false;

    if (!strcmp(action, "status")) {
        // just print status
        *result = g_string_assign(*result, tag->floating ? "on" : "off");
    } else {
        // asign new value and rearrange if needed
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

int tag_move_window_command(int argc, char** argv) {
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }
    HSTag* target = find_tag(argv[1]);
    if (!target) {
        return HERBST_INVALID_ARGUMENT;
    }
    tag_move_focused_client(target);
    return 0;
}

int tag_move_window_by_index_command(int argc, char** argv) {
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }
    bool skip_visible = false;
    if (argc >= 3 && !strcmp(argv[2], "--skip-visible")) {
        skip_visible = true;
    }
    HSTag* tag = get_tag_by_index_str(argv[1], skip_visible);
    if (!tag) {
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
    stack_remove_slice(client->tag->stack, client->slice);
    client->tag = target;
    stack_insert_slice(client->tag->stack, client->slice);
    ewmh_window_update_tag(client->window, client->tag);

    // refresh things, hide things, layout it, and then show it if needed
    if (monitor_source && !monitor_target) {
        // window is moved to unvisible tag
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



