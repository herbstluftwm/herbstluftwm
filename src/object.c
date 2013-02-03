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
#include <stdlib.h>

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
    obj->attributes = NULL;
    obj->attribute_count = 0;
    obj->children = NULL;
    return true;
}

void hsobject_free(HSObject* obj) {
    g_free(obj->attributes);
    g_list_free_full(obj->children, (GDestroyNotify)hsobjectchild_destroy);
}

HSObject* hsobject_create() {
    HSObject* obj = g_new(HSObject, 1);
    hsobject_init(obj);
    return obj;
}

void hsobject_destroy(HSObject* obj) {
    if (!obj) return;
    hsobject_free(obj);
    g_free(obj);
}

HSObject* hsobject_create_and_link(HSObject* parent, char* name) {
    HSObject* obj = hsobject_create();
    hsobject_link(parent, obj, name);
    return obj;
}

void hsobject_unlink_and_destroy(HSObject* parent, HSObject* child) {
    hsobject_unlink(parent, child);
    hsobject_destroy(child);
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
    char*    prefix;
    GString* curname;
    GString* output;
};

static void completion_helper(HSObjectChild* child, struct HSObjectComplChild* data) {
    g_string_assign(data->curname, child->name);
    g_string_append_c(data->curname, OBJECT_PATH_SEPARATOR);
    try_complete_prefix_partial(data->needle, data->curname->str, data->prefix, data->output);
}

void hsobject_complete_children(HSObject* obj, char* needle, char* prefix, GString* output) {
    struct HSObjectComplChild data = {
        needle,
        prefix,
        g_string_new(""),
        output
    };
    g_list_foreach(obj->children, (GFunc) completion_helper, &data);
    g_string_free(data.curname, true);
}

void hsobject_complete_attributes(HSObject* obj, char* needle, char* prefix,
                                GString* output) {
    for (int i = 0; i < obj->attribute_count; i++) {
        HSAttribute* attr = obj->attributes + i;
        try_complete_prefix(needle, attr->name, prefix, output);
    }
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
    // return 0 if they are identical
    return (child->child == obj) ? 0 : 1;
}

static void hsobject_unlink_helper(HSObject* parent, GCompareFunc f, void* data) {
    GList* elem = parent->children;
    while (elem) {
        elem = g_list_find_custom(elem, data, f);
        if (elem) {
            GList* next = elem->next;
            hsobjectchild_destroy((HSObjectChild*)elem->data);
            parent->children = g_list_delete_link(parent->children, elem);
            elem = next;
        }
    }
}

void hsobject_unlink(HSObject* parent, HSObject* child) {
    hsobject_unlink_helper(parent,
                           (GCompareFunc)child_check_object,
                           child);
}

void hsobject_unlink_by_name(HSObject* parent, char* name) {
    hsobject_unlink_helper(parent,
                           (GCompareFunc)child_check_name,
                           name);
}

void hsobject_link_rename(HSObject* parent, char* oldname, char* newname) {
    if (!strcmp(oldname, newname)) {
        return;
    }
    hsobject_unlink_by_name(parent, newname);
    GList* elem = g_list_find_custom(parent->children,
                                     oldname,
                                     (GCompareFunc)child_check_name);
    HSObjectChild* child = (HSObjectChild*)elem->data;
    g_free(child->name);
    child->name = g_strdup(newname);
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

HSAttribute* hsobject_find_attribute(HSObject* obj, char* name) {
    for (int i = 0; i < obj->attribute_count; i++) {
        if (!strcmp(name, obj->attributes[i].name)) {
            return obj->attributes + i;
        }
    }
    return NULL;
}

static void print_child_name(HSObjectChild* child, GString* output) {
    g_string_append_printf(output, "%s/\n", child->name);
}

void hsattribute_append_to_string(HSAttribute* attribute, GString* output) {
    switch (attribute->type) {
        case HSATTR_TYPE_BOOL:
            if (*(attribute->value.b)) {
                g_string_append_printf(output, "true");
            } else {
                g_string_append_printf(output, "false");
            }
            break;
        case HSATTR_TYPE_STRING:
            g_string_append_printf(output, "%s", (*(attribute->value.str))->str);
            break;
    }
}

GString* hsattribute_to_string(HSAttribute* attribute) {
    GString* str = g_string_new("");
    hsattribute_append_to_string(attribute, str);
    return str;
}

int list_objects_command(int argc, char* argv[], GString* output) {
    char* path = (argc < 2) ? "" : argv[1];
    char* unparsable;
    HSObject* obj = hsobject_parse_path_verbose(path, &unparsable, output);
    if (strcmp("", unparsable)) {
        return HERBST_INVALID_ARGUMENT;
    }
    // list attributes
    for (int i = 0; i < obj->attribute_count; i++) {
        HSAttribute* a = obj->attributes + i;
        g_string_append_printf(output, "%-20s = ", a->name);
        if (a->type == HSATTR_TYPE_STRING) {
            g_string_append_c(output, '\"');
        }
        hsattribute_append_to_string(a, output);
        if (a->type == HSATTR_TYPE_STRING) {
            g_string_append_c(output, '\"');
        }
        g_string_append_c(output, '\n');
    }
    // list children
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

HSObject* hsobject_by_path(char* path) {
    HSObject* obj = hsobject_parse_path(path, &path);
    if (!strcmp("", path)) {
        return obj;
    } else {
        // an invalid path was given if it was not parsed entirely
        return NULL;
    }
}

HSObject* hsobject_parse_path_verbose(char* path, char** unparsable,
                                      GString* output) {
    char* pathdup = strdup(path);
    char* curname = pathdup;
    char* lastname = "root";
    char seps[] = { OBJECT_PATH_SEPARATOR, '\0' };
    // replace all separators by null bytes
    g_strdelimit(curname, seps, '\0');
    HSObject* obj = hsobject_root();
    HSObject* child;
    // skip separator characters
    while (*path == OBJECT_PATH_SEPARATOR) {
        path++;
        curname++;
    }
    while (strcmp("", path)) {
        child = hsobject_find_child(obj, curname);
        if (!child) {
            if (output) {
                g_string_append_printf(output, "Invalid path \"%s\": ", path);
                g_string_append_printf(output, "No child \"%s\" in object %s\n",
                                       curname, lastname);
            }
            break;
        }
        lastname = curname;
        obj = child;
        // skip the name
        path    += strlen(curname);
        curname += strlen(curname);
        // skip separator characters
        while (*path == OBJECT_PATH_SEPARATOR) {
            path++;
            curname++;
        }
    }
    *unparsable = path;
    free(pathdup);
    return obj;
}

HSObject* hsobject_parse_path(char* path, char** unparsable) {
    return hsobject_parse_path_verbose(path, unparsable, NULL);
}

HSAttribute* hsattribute_parse_path_verbose(char* path, GString* output) {
    GString* object_error = g_string_new("");
    HSAttribute* attr;
    char* unparsable;
    HSObject* obj = hsobject_parse_path_verbose(path, &unparsable, object_error);
    if (obj == NULL || strchr(unparsable, OBJECT_PATH_SEPARATOR) != NULL) {
        // if there is still another path separator
        // then unparsable is more than just the attribute name.
        g_string_append(output, object_error->str);
        attr = NULL;
    } else {
        // if there is no path remaining separator, then unparsable contains
        // the attribute name
        attr = hsobject_find_attribute(obj, unparsable);
        if (!attr) {
            GString* obj_path = g_string_new(path);
            g_string_truncate(obj_path, unparsable - path);
            g_string_append_printf(output,
                "Unknown attribute \"%s\" in object \"%s\".\n",
                unparsable, obj_path->str);
            g_string_free(obj_path, true);
        }
    }
    g_string_free(object_error, true);
    return attr;
}

int print_object_tree_command(int argc, char* argv[], GString* output) {
    char* unparsable;
    char* path = (argc < 2) ? "" : argv[1];
    HSObjectChild oc = {
        .name = path,
        .child = hsobject_parse_path_verbose(path, &unparsable, output),
    };
    if (strcmp("", unparsable)) {
        return HERBST_INVALID_ARGUMENT;
    }
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

bool ATTR_ACCEPT_ALL(HSObject* obj, HSAttribute* attr) {
    (void) obj;
    (void) attr;
    return true;
}

bool ATTR_DENY_ALL(HSObject* obj, HSAttribute* attr) {
    (void) obj;
    (void) attr;
    return false;
}

void hsobject_set_attributes(HSObject* obj, HSAttribute* attributes) {
    // calculate new size
    size_t count;
    for (count = 0; attributes[count].name != NULL; count++)
        ;
    obj->attributes = g_renew(HSAttribute, obj->attributes, count);
    obj->attribute_count = count;
    memcpy(obj->attributes, attributes, count * sizeof(HSAttribute));
}

int hsattribute_get_command(int argc, char* argv[], GString* output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    HSAttribute* attr = hsattribute_parse_path_verbose(argv[1], output);
    if (!attr) {
        return HERBST_INVALID_ARGUMENT;
    }
    hsattribute_append_to_string(attr, output);
    return 0;
}

