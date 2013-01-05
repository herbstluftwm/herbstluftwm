/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "object.h"
#include "command.h"
#include "utils.h"
#include "assert.h"
#include "ipc-protocol.h"

#include <string.h>

typedef struct {
    char*       name;
    HSObject*   child;
} HSObjectChild;

static void hsobjectchild_destroy(HSObjectChild* oc);
static HSObjectChild* hsobjectchild_create(char* name, HSObject* obj);

static HSObject g_root_object;

void object_tree_init() {
    hsobject_init(&g_root_object);
}

void object_tree_destroy() {
    hsobject_free(&g_root_object);
}

HSObject* hsobject_root() {
    return &g_root_object;
}

bool hsobject_init(HSObject* obj) {
    obj->children = NULL;
    return true;
}

void hsobject_free(HSObject* obj) {
    g_list_free_full(obj->children, (GDestroyNotify)hsobjectchild_destroy);
}

HSObjectChild* hsobjectchild_create(char* name, HSObject* obj) {
    HSObjectChild* oc = g_new(HSObjectChild, 1);
    oc->name = g_strdup(name);
    oc->child = obj;
    return oc;
}

void hsobjectchild_destroy(HSObjectChild* oc) {
    if (!oc) return;
    g_free(oc->name);
    g_free(oc);
}

struct HSObjectComplChild {
    char*    needle;
    GString* output;
};

static void completion_helper(HSObjectChild* child, struct HSObjectComplChild* data) {
    try_complete(data->needle, child->name, data->output);
}

void hsobject_complete_children(HSObject* obj, char* needle, GString* output) {
    struct HSObjectComplChild data = { needle, output };
    g_list_foreach(obj->children, (GFunc) completion_helper, &data);
}

static int child_check_name(HSObjectChild* child, char* name) {
    return strcmp(child->name, name);
}

void hsobject_link(HSObject* parent, HSObject* child, char* name) {
    GList* elem = g_list_find_custom(parent->children, name,
                                     (GCompareFunc)child_check_name);
    if (!elem) {
        // create a new child node
        HSObjectChild* oc = hsobjectchild_create(name, child);
        parent->children = g_list_append(parent->children, oc);
    } else {
        // replace it
        HSObjectChild* oc = (HSObjectChild*) elem->data;
        oc->child = child;
    }
}

static int child_check_object(HSObjectChild* child, HSObject* obj) {
    return child->child == obj;
}

void hsobject_unlink(HSObject* parent, HSObject* child) {
    GList* elem = g_list_find_custom(parent->children, child,
                                     (GCompareFunc)child_check_object);
    if (elem) {
        hsobjectchild_destroy((HSObjectChild*)elem->data);
        parent->children = g_list_delete_link(parent->children, elem);
    }
}


HSObject* hsobject_find_child(HSObject* obj, char* name) {
    GList* elem = g_list_find_custom(obj->children, name,
                                     (GCompareFunc)child_check_name);
    if (elem) {
        return ((HSObjectChild*)(elem->data))->child;
    } else {
        return NULL;
    }
}

static void print_child_name(HSObjectChild* child, GString* output) {
    g_string_append_printf(output, "%s\n", child->name);
}

int list_objects_command(int argc, char* argv[], GString* output) {
    char* cmdname = argv[0];
    HSObject* obj = hsobject_root();
    while (SHIFT(argc,argv)) {
        obj = hsobject_find_child(obj, argv[0]);
        if (!obj) {
            g_string_append_printf(output, "%s: Can not find object \"%s\"\n",
                                   cmdname, argv[0]);
            return HERBST_INVALID_ARGUMENT;
        }
    }
    // list object contents
    // TODO: list attributes
    // list children
    g_string_append_printf(output, "children:\n");
    g_list_foreach(obj->children, (GFunc) print_child_name, output);
    return 0;
}

static void object_append_caption(HSTree tree, GString* output) {
    HSObjectChild* oc = (HSObjectChild*) tree;
    g_string_append(output, oc->name);
}

static size_t object_child_count(HSTree tree) {
    HSObjectChild* oc = (HSObjectChild*) tree;
    return g_list_length(oc->child->children);
}

static HSTreeInterface object_nth_child(HSTree tree, size_t idx) {
    HSObjectChild* oc = (HSObjectChild*) tree;
    assert(oc->child);
    HSTreeInterface intf = {
        .nth_child  = object_nth_child,
        .data       = (HSTree) g_list_nth_data(oc->child->children, idx),
        .destructor = NULL,
        .child_count    = object_child_count,
        .append_caption = object_append_caption,
    };
    return intf;
}

int print_object_tree_command(int argc, char* argv[], GString* output) {
    HSObjectChild oc = { .name = "", .child = hsobject_root() };
    HSTreeInterface intf = {
        .nth_child  = object_nth_child,
        .data       = &oc,
        .destructor = NULL,
        .child_count    = object_child_count,
        .append_caption = object_append_caption,
    };
    tree_print_to(&intf, output);
    return 0;
}

