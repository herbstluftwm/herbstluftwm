/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "object.h"
#include "command.h"
#include "utils.h"
#include "assert.h"
#include "globals.h"
#include "ipc-protocol.h"

#include <iostream>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

namespace herbstluft {

Object::Object(const std::string &name)
    : Directory(name),
      nameAttribute_("name", Type::ATTRIBUTE_STRING, true, false)
{
    wireAttributes({ &nameAttribute_ });
}

bool Object::readable(const std::string &attr) const {
    auto it = attribs_.find(attr);
    if (it != attribs_.end()) {
        return it->second->readable();
    }
    return false; // TODO: else throw
}

std::string Object::read(const std::string &attr) const {
    if (attr == "name")
        return name_;

    auto it = attribs_.find(attr);
    if (it != attribs_.end())
        return it->second->str();
    return {}; // TODO: throw
}

bool Object::writeable(const std::string &attr) const {
    auto it = attribs_.find(attr);
    if (it != attribs_.end()) {
        return it->second->readable();
    }
    return false; // TODO: throw
}

void Object::write(const std::string &attr, const std::string &value) {
    auto it = attribs_.find(attr);
    if (it != attribs_.end()) {
        if (it->second->writeable())
            it->second->change(value);
        // TODO: else throw
    } else {
        // TODO: throw
    }
}

void Object::trigger(const std::string &action, const Arg &args) {
    // do nothing, there is no default behavior for actions.
    // TODO: throw; if we got here, there was an error, e.g. typo on user's side
}

void Object::wireAttributes(std::vector<Attribute*> attrs)
{
    for (auto attr : attrs) {
        attr->setOwner(this);
        attribs_[attr->name()] = attr;
    }
}

void Object::wireActions(std::vector<Action*> actions)
{
    for (auto action : actions) {
        action->setOwner(this);
        actions_[action->name()] = action;
    }
}

void Object::print(const std::string &prefix)
{
    std::cout << prefix << "==== " << typestr() << " " << name_ << ":" << std::endl;
    if (!children_.empty()) {
        std::cout << prefix << "Children:" << std::endl;
        for (auto it : children_) {
            it.second->print(prefix + "\t| ");
        }
        std::cout << prefix << std::endl;
    }
    if (!attribs_.empty()) {
        std::cout << prefix << "Attributes:" << std::endl;
        for (auto it : attribs_) {
            std::cout << prefix << "\t" << it.first
                      << " (" << it.second->typestr() << ")";
            if (it.second->readable())
                std::cout << "\tr(" << it.second->read() << ")";
            if (it.second->writeable())
                std::cout << "\tw";
            std::cout << std::endl;
        }
    }
    if (!actions_.empty()) {
        std::cout << prefix << "Actions:" << std::endl;
        std::cout << prefix;
        for (auto it : actions_) {
            std::cout << "\t" << it.first;
        }
        std::cout << std::endl;
    }
    if (!hooks_.empty()) {
        std::cout << prefix << "Current hooks:" << std::endl;
        for (auto it : hooks_) {
            std::cout << prefix << "\t" << it.first << std::endl;
        }
    }
}

}

using namespace herbstluft;

typedef struct {
    char*       name;
    HSObject*   child;
} HSObjectChild;

static void hsobjectchild_destroy(HSObjectChild* oc);
static HSObjectChild* hsobjectchild_create(const char* name, HSObject* obj);
static void hsattribute_free(HSAttribute* attr);

static HSObject g_root_object;
static HSObject* g_tmp_object;

void object_tree_init() {
    hsobject_init(&g_root_object);
    g_tmp_object = hsobject_create_and_link(&g_root_object, TMP_OBJECT_PATH);

}

void object_tree_destroy() {
    hsobject_unlink_and_destroy(&g_root_object, g_tmp_object);
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
    for (int i = 0; i < obj->attribute_count; i++) {
        hsattribute_free(obj->attributes + i);
    }
    g_free(obj->attributes);
    g_list_free_full(obj->children, (GDestroyNotify)hsobjectchild_destroy);
}

static void hsattribute_free(HSAttribute* attr) {
    if (attr->user_data) {
        g_free((char*)attr->name);
        if (attr->type == HSATTR_TYPE_STRING) {
            g_string_free(attr->user_data->str, true);
        }
        g_free(attr->user_data);
    }
    if (attr->unparsed_value) {
        g_string_free(attr->unparsed_value, true);
    }
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

HSObject* hsobject_create_and_link(HSObject* parent, const char* name) {
    HSObject* obj = hsobject_create();
    hsobject_link(parent, obj, name);
    return obj;
}

void hsobject_unlink_and_destroy(HSObject* parent, HSObject* child) {
    hsobject_unlink(parent, child);
    hsobject_destroy(child);
}

static HSObjectChild* hsobjectchild_create(const char* name, HSObject* obj) {
    HSObjectChild* oc = g_new(HSObjectChild, 1);
    oc->name = g_strdup(name);
    oc->child = obj;
    return oc;
}

static void hsobjectchild_destroy(HSObjectChild* oc) {
    if (!oc) return;
    g_free((char*)oc->name);
    g_free(oc);
}

struct HSObjectComplChild {
    const char*    needle;
    const char*    prefix;
    GString* curname;
    GString* output;
};

static void completion_helper(HSObjectChild* child, struct HSObjectComplChild* data) {
    g_string_assign(data->curname, child->name);
    g_string_append_c(data->curname, OBJECT_PATH_SEPARATOR);
    try_complete_prefix_partial(data->needle, data->curname->str, data->prefix, data->output);
}

void hsobject_complete_children(HSObject* obj, const char* needle, const char* prefix, GString* output) {
    struct HSObjectComplChild data = {
        needle,
        prefix,
        g_string_new(""),
        output
    };
    g_list_foreach(obj->children, (GFunc) completion_helper, &data);
    g_string_free(data.curname, true);
}

void hsobject_complete_attributes(HSObject* obj, bool user_only, const char* needle,
                                  const char* prefix, GString* output) {
    for (int i = 0; i < obj->attribute_count; i++) {
        HSAttribute* attr = obj->attributes + i;
        if (user_only && !attr->user_attribute) {
            // do not complete default attributes if user_only is set
            continue;
        }
        try_complete_prefix(needle, attr->name, prefix, output);
    }
}

static int child_check_name(HSObjectChild* child, char* name) {
    return strcmp(child->name, name);
}

void hsobject_link(HSObject* parent, HSObject* child, const char* name) {
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

static void hsobject_unlink_helper(HSObject* parent, GCompareFunc f, const void* data) {
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

void hsobject_unlink_by_name(HSObject* parent, const char* name) {
    hsobject_unlink_helper(parent,
                           (GCompareFunc)child_check_name,
                           name);
}

void hsobject_link_rename(HSObject* parent, char* oldname, char* newname) {
    if (!strcmp(oldname, newname)) {
        return;
    }
    // remove object with target name
    hsobject_unlink_by_name(parent, newname);
    GList* elem = g_list_find_custom(parent->children,
                                     oldname,
                                     (GCompareFunc)child_check_name);
    HSObjectChild* child = (HSObjectChild*)elem->data;
    g_free(child->name);
    child->name = g_strdup(newname);
}

void hsobject_link_rename_object(HSObject* parent, HSObject* child, char* newname) {
    // remove occurrences of that object
    hsobject_unlink(parent, child);
    // link it again (replacing any object with newname)
    hsobject_link(parent, child, newname);
}

HSObject* hsobject_find_child(HSObject* obj, const char* name) {
    GList* elem = g_list_find_custom(obj->children, name,
                                     (GCompareFunc)child_check_name);
    if (elem) {
        return ((HSObjectChild*)(elem->data))->child;
    } else {
        return NULL;
    }
}

HSAttribute* hsobject_find_attribute(HSObject* obj, const char* name) {
    for (int i = 0; i < obj->attribute_count; i++) {
        if (!strcmp(name, obj->attributes[i].name)) {
            return obj->attributes + i;
        }
    }
    return NULL;
}

void hsobject_set_attributes_always_callback(HSObject* obj) {
    for (int i = 0; i < obj->attribute_count; i++) {
        obj->attributes[i].always_callback = true;
    }
}

static void print_child_name(HSObjectChild* child, GString* output) {
    g_string_append_printf(output, "  %s%c\n", child->name, OBJECT_PATH_SEPARATOR);
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
        case HSATTR_TYPE_INT:
            g_string_append_printf(output, "%d", *attribute->value.i);
            break;
        case HSATTR_TYPE_UINT:
            g_string_append_printf(output, "%u", *attribute->value.u);
            break;
        case HSATTR_TYPE_STRING:
            g_string_append_printf(output, "%s", (*attribute->value.str)->str);
            break;
        case HSATTR_TYPE_COLOR:
            g_string_append_printf(output, "%s", attribute->unparsed_value->str);
            break;
        case HSATTR_TYPE_CUSTOM:
            attribute->value.custom(attribute->data ? attribute->data
                                                    : attribute->object->data, output);
            break;
        case HSATTR_TYPE_CUSTOM_INT:
            g_string_append_printf(output, "%d",
                attribute->value.custom_int(attribute->data ? attribute->data
                                                            : attribute->object->data));
            break;
    }
}

GString* hsattribute_to_string(HSAttribute* attribute) {
    GString* str = g_string_new("");
    hsattribute_append_to_string(attribute, str);
    return str;
}

int attr_command(int argc, char* argv[], GString* output) {
    const char* path = (argc < 2) ? "" : argv[1];
    const char* unparsable;
    GString* errormsg = g_string_new("");
    HSObject* obj = hsobject_parse_path_verbose(path, &unparsable, errormsg);
    HSAttribute* attribute = NULL;
    if (strcmp("", unparsable)) {
        // if object could not be parsed, try attribute
        attribute = hsattribute_parse_path_verbose(path, errormsg);
        obj = NULL;
    }
    if (!obj && !attribute) {
        // if nothing was found
        g_string_append(output, errormsg->str);
        g_string_free(errormsg, true);
        return HERBST_INVALID_ARGUMENT;
    } else {
        g_string_free(errormsg, true);
    }
    char* new_value = (argc >= 3) ? argv[2] : NULL;
    if (obj && new_value) {
        g_string_append_printf(output,
            "%s: Can not assign value \"%s\" to object \"%s\",",
            argv[0], new_value, path);
    } else if (obj && !new_value) {
        // list children
        int childcount = g_list_length(obj->children);
        g_string_append_printf(output, "%d children%c\n", childcount,
                               childcount ? ':' : '.');
        g_list_foreach(obj->children, (GFunc) print_child_name, output);
        if (childcount > 0) {
            g_string_append_printf(output, "\n");
        }
        // list attributes
        g_string_append_printf(output, "%zu attributes", obj->attribute_count);
        if (obj->attribute_count > 0) {
            g_string_append_printf(output, ":\n");
            g_string_append_printf(output, " .---- type\n");
            g_string_append_printf(output, " | .-- writeable\n");
            g_string_append_printf(output, " V V\n");
        } else {
            g_string_append_printf(output, ".\n");
        }
        for (int i = 0; i < obj->attribute_count; i++) {
            HSAttribute* a = obj->attributes + i;
            char write = hsattribute_is_read_only(a) ? '-' : 'w';
            char t = hsattribute_type_indicator(a->type);
            g_string_append_printf(output, " %c %c %-20s = ", t, write, a->name);
            if (a->type == HSATTR_TYPE_STRING) {
                g_string_append_c(output, '\"');
            }
            hsattribute_append_to_string(a, output);
            if (a->type == HSATTR_TYPE_STRING) {
                g_string_append_c(output, '\"');
            }
            g_string_append_c(output, '\n');
        }
    } else if (new_value) { // && (attribute)
        return hsattribute_assign(attribute, new_value, output);
    } else { // !new_value && (attribute)
        hsattribute_append_to_string(attribute, output);
    }
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
        /* .nth_child  = */ object_nth_child,
        /* .child_count    = */ object_child_count,
        /* .append_caption = */ object_append_caption,
        /* .data       = */ (HSTree) g_list_nth_data(oc->child->children, idx),
        /* .destructor = */ NULL,
    };
    return intf;
}

HSObject* hsobject_by_path(char* path) {
    const char* unparsable;
    HSObject* obj = hsobject_parse_path(path, &unparsable);
    if (!strcmp("", unparsable)) {
        return obj;
    } else {
        // an invalid path was given if it was not parsed entirely
        return NULL;
    }
}

HSObject* hsobject_parse_path_verbose(const char* path, const char** unparsable,
                                      GString* output) {
    const char* origpath = path;
    char* pathdup = strdup(path);
    char* curname = pathdup;
    const char* lastname = "root";
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
                g_string_append_printf(output, "Invalid path \"%s\": ", origpath);
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

HSObject* hsobject_parse_path(const char* path, const char** unparsable) {
    return hsobject_parse_path_verbose(path, unparsable, NULL);
}

HSAttribute* hsattribute_parse_path_verbose(const char* path, GString* output) {
    GString* object_error = g_string_new("");
    HSAttribute* attr;
    const char* unparsable;
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

HSAttribute* hsattribute_parse_path(const char* path) {
    GString* out = g_string_new("");
    HSAttribute* attr = hsattribute_parse_path_verbose(path, out);
    if (!attr) {
        HSError("Cannot parse %s: %s", path, out->str);
    }
    g_string_free(out, true);
    return attr;
}

int print_object_tree_command(int argc, char* argv[], GString* output) {
    const char* unparsable;
    const char* path = (argc < 2) ? "" : argv[1];
    HSObjectChild oc = {
        (char*)path,
        hsobject_parse_path_verbose(path, &unparsable, output),
    };
    if (strcmp("", unparsable)) {
        return HERBST_INVALID_ARGUMENT;
    }
    HSTreeInterface intf = {
        /* .nth_child  = */ object_nth_child,
        /* .child_count    = */ object_child_count,
        /* .append_caption = */ object_append_caption,
        /* .data       = */ &oc,
        /* .destructor = */ NULL,
    };
    tree_print_to(&intf, output);
    return 0;
}

void hsobject_set_attributes(HSObject* obj, HSAttribute* attributes) {
    // calculate new size
    size_t count;
    for (count = 0; attributes[count].name != NULL; count++)
        ;
    obj->attributes = g_renew(HSAttribute, obj->attributes, count);
    obj->attribute_count = count;
    memcpy(obj->attributes, attributes, count * sizeof(HSAttribute));
    for (int i = 0; i < count; i++) {
        obj->attributes[i].object = obj;
    }
}

int hsattribute_get_command(int argc, const char* argv[], GString* output) {
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

int hsattribute_set_command(int argc, char* argv[], GString* output) {
    if (argc < 3) {
        return HERBST_NEED_MORE_ARGS;
    }
    HSAttribute* attr = hsattribute_parse_path_verbose(argv[1], output);
    if (!attr) {
        return HERBST_INVALID_ARGUMENT;
    }
    return hsattribute_assign(attr, argv[2], output);
}

bool hsattribute_is_read_only(HSAttribute* attr) {
    bool custom = attr->type == HSATTR_TYPE_CUSTOM
                || attr->type == HSATTR_TYPE_CUSTOM_INT;
    assert(!(custom && attr->on_change));
    if (custom) return attr->change_custom == NULL;
    else return attr->on_change == NULL;
}

int hsattribute_assign(HSAttribute* attr, const char* new_value_str, GString* output) {
    if (hsattribute_is_read_only(attr)) {
        g_string_append_printf(output,
            "Can not write read-only attribute \"%s\"\n",
            attr->name);
        return HERBST_FORBIDDEN;
    }

    bool error = false;
    HSAttributeValue new_value, old_value;
    bool nothing_to_do = false;

#define ATTR_DO_ASSIGN_COMPARE(NAME,MEM) \
        do { \
            if (error) { \
                g_string_append_printf(output, \
                                       "Can not parse " NAME " from \"%s\"", \
                                       new_value_str); \
            } else { \
                old_value.MEM = *attr->value.MEM; \
                if (old_value.MEM == new_value.MEM) { \
                    nothing_to_do = true; \
                } else { \
                    *attr->value.MEM = new_value.MEM; \
                } \
            } \
        } while (0);
    // change the value and backup the old value
    switch (attr->type) {
        case HSATTR_TYPE_BOOL:
            new_value.b = string_to_bool_error(new_value_str,
                                             *attr->value.b,
                                             &error);
            ATTR_DO_ASSIGN_COMPARE("boolean", b);
            break;

        case HSATTR_TYPE_INT:
            error = (1 != sscanf(new_value_str, "%d", &new_value.i));
            ATTR_DO_ASSIGN_COMPARE("integer", i);
            break;

        case HSATTR_TYPE_UINT:
            error = (1 != sscanf(new_value_str, "%u", &new_value.u));
            ATTR_DO_ASSIGN_COMPARE("unsigned integer", u);
            break;

        case HSATTR_TYPE_STRING:
            if (!strcmp(new_value_str, (*attr->value.str)->str)) {
                nothing_to_do = true;
            } else {
                old_value.str = g_string_new((*attr->value.str)->str);
                g_string_assign(*attr->value.str, new_value_str);
            }
            break;

        case HSATTR_TYPE_COLOR:
            error = !Color::convert(new_value_str, new_value.color);
            if (error) {
                g_string_append_printf(output,
                    "\"%s\" is not a valid color.", new_value_str);
                break;
            }
            if (!strcmp(new_value_str, (attr->unparsed_value)->str)) {
                nothing_to_do = true;
            } else {
                old_value.color = *attr->value.color;
                *attr->value.color = new_value.color;
            }
            break;

        case HSATTR_TYPE_CUSTOM: break;
        case HSATTR_TYPE_CUSTOM_INT: break;
    }
    if (error) {
        return HERBST_INVALID_ARGUMENT;
    }
    if (attr->always_callback) {
        nothing_to_do = false; // pretend that there was a change
    }
    if (nothing_to_do) {
        return 0;
    }

    GString* old_unparsed_value = attr->unparsed_value;
    if (attr->unparsed_value) attr->unparsed_value = g_string_new(new_value_str);

    // ask the attribute about the change
    GString* errormsg = NULL;
    if (attr->on_change) errormsg = attr->on_change(attr);
    else errormsg = attr->change_custom(attr, new_value_str);
    int exit_status = 0;
    if (errormsg && errormsg->len > 0) {
        exit_status = HERBST_INVALID_ARGUMENT;
        // print the message
        if (errormsg->str[errormsg->len - 1] == '\n') {
            g_string_truncate(errormsg, errormsg->len - 1);
        }
        g_string_append_printf(output,
            "Can not write attribute \"%s\": %s\n",
            attr->name,
            errormsg->str);
        g_string_free(errormsg, true);
        // restore old value
        if (old_unparsed_value) {
            g_string_free(attr->unparsed_value, true);
            attr->unparsed_value = old_unparsed_value;
        }
        switch (attr->type) {
            case HSATTR_TYPE_BOOL: *attr->value.b = old_value.b; break;
            case HSATTR_TYPE_INT:  *attr->value.i = old_value.i; break;
            case HSATTR_TYPE_UINT: *attr->value.u = old_value.u; break;
            case HSATTR_TYPE_STRING:
                g_string_assign(*attr->value.str, old_value.str->str);
                break;
            case HSATTR_TYPE_COLOR:
                *attr->value.color = old_value.color; break;
                break;
            case HSATTR_TYPE_CUSTOM: break;
            case HSATTR_TYPE_CUSTOM_INT: break;
        }
    }
    // free old_value
    switch (attr->type) {
        case HSATTR_TYPE_BOOL: break;
        case HSATTR_TYPE_INT:  break;
        case HSATTR_TYPE_UINT:  break;
        case HSATTR_TYPE_STRING:
            g_string_free(old_value.str, true);
            break;
        case HSATTR_TYPE_COLOR:
            g_string_assign(attr->unparsed_value, new_value_str);
            break;
        case HSATTR_TYPE_CUSTOM: break;
        case HSATTR_TYPE_CUSTOM_INT: break;
    }
    if (old_unparsed_value) {
        g_string_free(old_unparsed_value, true);
    }
    return exit_status;
}


int substitute_command(int argc, char* argv[], GString* output) {
    // usage: substitute identifier attribute command [args ...]
    //            0         1           2       3
    if (argc < 4) {
        return HERBST_NEED_MORE_ARGS;
    }
    char* identifier = argv[1];
    HSAttribute* attribute = hsattribute_parse_path_verbose(argv[2], output);
    if (!attribute) {
        return HERBST_INVALID_ARGUMENT;
    }
    GString* attribute_string = hsattribute_to_string(attribute);
    char* repl = attribute_string->str;

    (void) SHIFT(argc, argv); // remove command name
    (void) SHIFT(argc, argv); // remove identifier
    (void) SHIFT(argc, argv); // remove attribute

    int status = call_command_substitute(identifier, repl, argc, argv, output);
    g_string_free(attribute_string, true);
    return status;
}

GString* ATTR_ACCEPT_ALL(HSAttribute* attr) {
    (void) attr;
    return NULL;
}

int compare_command(int argc, char* argv[], GString* output) {
    // usage: compare attribute operator constant
    if (argc < 4) {
        return HERBST_NEED_MORE_ARGS;
    }
    HSAttribute* attr = hsattribute_parse_path_verbose(argv[1], output);
    if (!attr) {
        return HERBST_INVALID_ARGUMENT;
    }
    char* op = argv[2];
    char* rvalue = argv[3];
    if (attr->type == HSATTR_TYPE_INT
        || attr->type == HSATTR_TYPE_UINT
        || attr->type == HSATTR_TYPE_CUSTOM_INT)
    {
        long long l;
        long long r;
        if (1 != sscanf(rvalue, "%lld", &r)) {
            g_string_append_printf(output,
                                   "Can not parse integer from \"%s\"\n",
                                   rvalue);
            return HERBST_INVALID_ARGUMENT;
        }
        switch (attr->type) {
            case HSATTR_TYPE_INT:  l = *attr->value.i; break;
            case HSATTR_TYPE_UINT: l = *attr->value.u; break;
            case HSATTR_TYPE_CUSTOM_INT:
                l = attr->value.custom_int(attr->data ? attr->data : attr->object->data);
                break;
            default: return HERBST_UNKNOWN_ERROR; break;
        }
        struct {
            const char* name;
            bool  result;
        } eval[] = {
            { "=",  l == r  },
            { "!=", l != r  },
            { "le", l <= r  },
            { "lt", l <  r  },
            { "ge", l >= r  },
            { "gt", l >  r  },
        };
        int result = -1;
        for (int i = 0; i < LENGTH(eval); i++) {
            if (!strcmp(eval[i].name, op)) {
                result = !eval[i].result; // make false -> 1, true -> 0
            }
        }
        if (result == -1) {
            g_string_append_printf(output, "Invalid operator \"%s\"", op);
            result = HERBST_INVALID_ARGUMENT;
        }
        return result;
    } else if (attr->type == HSATTR_TYPE_BOOL) {
        bool l = *attr->value.b;
        bool error = false;
        bool r = string_to_bool_error(rvalue, l, &error);
        if (error) {
            g_string_append_printf(output, "Can not parse boolean from \"%s\"\n", rvalue);
            return HERBST_INVALID_ARGUMENT;
        }
        if (!strcmp("=", op)) return !(l == r);
        if (!strcmp("!=", op)) return !(l != r);
        g_string_append_printf(output, "Invalid boolean operator \"%s\"", op);
        return HERBST_INVALID_ARGUMENT;
    } else if (attr->type == HSATTR_TYPE_COLOR) {
        auto l = *attr->value.color;
        auto r = Color::fromStr(rvalue);
        if (!strcmp("=", op)) return !(l == r);
        if (!strcmp("!=", op)) return !(l != r);
        g_string_append_printf(output, "Invalid color operator \"%s\"", op);
        return HERBST_INVALID_ARGUMENT;
    } else { // STRING or CUSTOM
        GString* l;
        bool free_l = false;
        if (attr->type == HSATTR_TYPE_STRING) {
            l = *attr->value.str;
        } else { // TYPE == CUSTOM
            l = g_string_new("");
            attr->value.custom(attr->data ? attr->data : attr->object->data, l);
            free_l = true;
        }
        bool equals = !strcmp(l->str, rvalue);
        int status;
        if (!strcmp("=", op)) status = !equals;
        else if (!strcmp("!=", op)) status = equals;
        else status = -1;
        if (free_l) {
            g_string_free(l, true);
        }
        if (status == -1) {
            g_string_append_printf(output, "Invalid string operator \"%s\"", op);
            return HERBST_INVALID_ARGUMENT;
        }
        return status;
    }
}

char hsattribute_type_indicator(int type) {
    switch (type) {
        case HSATTR_TYPE_BOOL:      return 'b';
        case HSATTR_TYPE_UINT:      return 'u';
        case HSATTR_TYPE_INT:       return 'i';
        case HSATTR_TYPE_STRING:    return 's';
        case HSATTR_TYPE_CUSTOM:    return 's';
        case HSATTR_TYPE_CUSTOM_INT:return 'i';
        case HSATTR_TYPE_COLOR:     return 'c';
    }
    return '?';
}

int userattribute_command(int argc, char* argv[], GString* output) {
    if (argc < 3) {
        return HERBST_NEED_MORE_ARGS;
    }
    char* type_str = argv[1];
    char* path = argv[2];
    const char* unparsable;
    GString* errormsg = g_string_new("");
    HSObject* obj = hsobject_parse_path_verbose(path, &unparsable, errormsg);
    if (obj == NULL || strchr(unparsable, OBJECT_PATH_SEPARATOR) != NULL) {
        g_string_append(output, errormsg->str);
        g_string_free(errormsg, true);
        return HERBST_INVALID_ARGUMENT;
    } else {
        g_string_free(errormsg, true);
    }
    // check for an already existing attribute
    if (hsobject_find_attribute(obj, unparsable)) {
        g_string_append_printf(output, "Error: an attribute called \"%s\" already exists\n",
                               unparsable);
        return HERBST_FORBIDDEN;
    }
    // do not check for children with that name, because they must not start
    // with the USER_ATTRIBUTE_PREFIX.
    // now create a new attribute named unparsable at obj
    const char* prefix = USER_ATTRIBUTE_PREFIX;
    if (strncmp(unparsable, prefix, strlen(prefix))) {
        g_string_append(output, "Error: the name of user attributes has to ");
        g_string_append_printf(output, "start with \"%s\" but yours is \"%s\"\n",
                                       prefix, unparsable);
        return HERBST_INVALID_ARGUMENT;
    }
    HSAttribute* attr = hsattribute_create(obj, unparsable, type_str, output);
    if (!attr) {
        return HERBST_INVALID_ARGUMENT;
    }
    attr->user_attribute = true;
    return 0;
}

HSAttribute* hsattribute_create(HSObject* obj, const char* name, char* type_str,
                                GString* output)
{
    struct {
        const char* name;
        int   type;
    } types[] = {
        { "bool",   HSATTR_TYPE_BOOL    },
        { "uint",   HSATTR_TYPE_UINT    },
        { "int",    HSATTR_TYPE_INT     },
        { "string", HSATTR_TYPE_STRING  },
        { "color",  HSATTR_TYPE_COLOR   },
    };
    int type = -1;
    for (int i = 0; i < LENGTH(types); i++) {
        if (!strcmp(type_str, types[i].name)) {
            type = types[i].type;
            break;
        }
    }
    if (type < 0) {
        g_string_append_printf(output, "Unknown attribute type \"%s\"\n",
                               type_str);
        return NULL;
    }
    size_t count = obj->attribute_count + 1;
    obj->attributes = g_renew(HSAttribute, obj->attributes, count);
    obj->attribute_count = count;
    // initialize object
    HSAttribute* attr = obj->attributes + count - 1;
    memset(attr, 0, sizeof(*attr));
    attr->object = obj;
    attr->type = (HSAttributeType)type;
    attr->name = g_strdup(name);
    attr->on_change = ATTR_ACCEPT_ALL;
    attr->user_attribute = false;
    attr->user_data = g_new(HSAttributeValue, 1);
    switch (type) {
        case HSATTR_TYPE_BOOL:
            attr->user_data->b = false;
            attr->value.b = &attr->user_data->b;
            break;
        case HSATTR_TYPE_INT:
            attr->user_data->i = 0;
            attr->value.i = &attr->user_data->i;
            break;
        case HSATTR_TYPE_UINT:
            attr->user_data->u = 0;
            attr->value.u = &attr->user_data->u;
            break;
        case HSATTR_TYPE_STRING:
            attr->user_data->str = g_string_new("");
            attr->value.str = &attr->user_data->str;
            break;
        case HSATTR_TYPE_COLOR:
            attr->user_data->color = Color::fromStr("#000000");
            attr->unparsed_value = g_string_new("#000000");
            attr->value.color = &attr->user_data->color;
        default:
            break;
    }
    return attr;
}

int userattribute_remove_command(int argc, char* argv[], GString* output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    char* path = argv[1];
    HSAttribute* attr = hsattribute_parse_path_verbose(path, output);
    if (!attr) {
        return HERBST_INVALID_ARGUMENT;
    }
    if (!attr->user_attribute) {
        g_string_append_printf(output, "Can only user-defined attributes, "
                                       "but \"%s\" is not user-defined.\n",
                               path);
        return HERBST_FORBIDDEN;
    }
    return userattribute_remove(attr) ? 0 : HERBST_UNKNOWN_ERROR;
}

bool userattribute_remove(HSAttribute* attr) {
    HSObject* obj = attr->object;
    int idx = attr - obj->attributes;
    if (idx < 0 || idx >= obj->attribute_count) {
        fprintf(stderr, "Assertion 0 <= idx < count failed.\n");
        return false;
    }
    hsattribute_free(attr);
    // remove it from buf
    size_t count = obj->attribute_count - 1;
    size_t bytes = (count - idx) * sizeof(HSAttribute);
    memmove(obj->attributes + idx, obj->attributes + idx + 1, bytes);
    obj->attributes = g_renew(HSAttribute, obj->attributes, count);
    obj->attribute_count = count;
    return 0;
}

#define FORMAT_CHAR '%'

int sprintf_command(int argc, char* argv[], GString* output) {
    // usage: sprintf IDENTIFIER FORMAT [Params...] COMMAND [ARGS ...]
    if (argc < 4) {
        return HERBST_NEED_MORE_ARGS;
    }
    char* identifier = argv[1];
    char* format = argv[2];
    (void) SHIFT(argc, argv);
    (void) SHIFT(argc, argv);
    (void) SHIFT(argc, argv);
    GString* repl = g_string_new("");
    int nextarg = 0; // next argument to consider
    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == FORMAT_CHAR) {
            // FORMAT_CHAR is our format character
            // '%' is the printf format character
            switch (format[i+1]) {
                case FORMAT_CHAR:
                    g_string_append(repl, "%%");
                    break;

                case 's': {
                    if (nextarg >= (argc - 1)) {
                        g_string_append_printf(output,
                            "Error: Too few parameters. A %dth parameter missing. "
                            "(treating \"%s\" as the command to execute)\n",
                            nextarg, argv[argc - 1]);
                        g_string_free(repl, true);
                        return HERBST_INVALID_ARGUMENT;
                    }
                    HSAttribute* attr;
                    attr = hsattribute_parse_path_verbose(argv[nextarg], output);
                    if (!attr) {
                        g_string_free(repl, true);
                        return HERBST_INVALID_ARGUMENT;
                    }
                    GString* gs = hsattribute_to_string(attr);
                    g_string_append(repl, gs->str);
                    g_string_free(gs, true);
                    nextarg++;
                    break;
                }

                default:
                    g_string_append_printf(output,
                        "Error: unknown format specifier \'%c\' in format "
                        "\"%s\" at position %d\n",
                        format[i+1] ? format[i+1] : '?', format, i);
                    g_string_free(repl, true);
                    return HERBST_INVALID_ARGUMENT;
                    break;
            }
            i++;
        } else {
            g_string_append_c(repl, format[i]);
        }
    }
    int cmdc = argc - nextarg;
    char** cmdv = argv + nextarg;
    int status;
    status = call_command_substitute(identifier, repl->str, cmdc, cmdv, output);
    g_string_free(repl, true);
    return status;
}

int tmpattribute_command(int argc, char* argv[], GString* output) {
    // usage: tmp type IDENTIFIER COMMAND [ARGS...]
    if (argc < 4) {
        return HERBST_NEED_MORE_ARGS;
    }
    static int tmpcount = 0;
    tmpcount++;
    char* name = g_strdup_printf("%stmp%d", USER_ATTRIBUTE_PREFIX, tmpcount);
    // attr may change, so only remember the object
    HSAttribute* attr = hsattribute_create(g_tmp_object, name, argv[1], output);
    if (!attr) {
        tmpcount--;
        g_free(name);
        return HERBST_INVALID_ARGUMENT;
    }
    HSObject* obj = attr->object;
    char* path = g_strdup_printf("%s%c%s", TMP_OBJECT_PATH,
                                 OBJECT_PATH_SEPARATOR, name);
    int status = call_command_substitute(argv[2], path, argc - 3, argv + 3, output);
    userattribute_remove(hsobject_find_attribute(obj, name));
    g_free(name);
    g_free(path);
    tmpcount--;
    return status;
}

