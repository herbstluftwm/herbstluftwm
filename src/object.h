/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HS_OBJECT_H_
#define __HS_OBJECT_H_

#include <stdbool.h>
#include <glib.h>

typedef struct HSObject {
    struct HSAttribute* attributes;
    size_t              attribute_count;
    GList*              children; // list of HSObjectChild
} HSObject;

typedef struct HSAttribute {
    enum  {
        HSATTR_TYPE_BOOL,
        HSATTR_TYPE_STRING,
    } type;                   /* the datatype */
    char*  name;              /* name as it is displayed to the user */
    union {
        bool*       b;
        GString**   str;
    } value;
    /** on_change is called after the user changes the value. If this
     * function returns false, the old value will be restored */
    bool (*on_change)(HSObject* obj, struct HSAttribute* attr);
} HSAttribute;

#define ATTRIBUTE_BOOL(N, V, CHANGE) \
    { HSATTR_TYPE_BOOL, (N), { .b = &(V) }, (CHANGE) }
#define ATTRIBUTE_STRING(N, V, CHANGE) \
    { HSATTR_TYPE_STRING, (N), { .str = &(V) }, (CHANGE) }

#define ATTRIBUTE_LAST { .name = NULL }

void object_tree_init();
void object_tree_destroy();

HSObject* hsobject_root();

bool hsobject_init(HSObject* obj);
void hsobject_free(HSObject* obj);
void hsobject_link(HSObject* parent, HSObject* child, char* name);
void hsobject_unlink(HSObject* parent, HSObject* child);
void hsobject_unlink_by_name(HSObject* parent, char* name);
void hsobject_link_rename(HSObject* parent, char* oldname, char* newname);

void hsobject_set_attributes(HSObject* obj, HSAttribute* attributes);

bool    ATTR_ACCEPT_ALL (HSObject* obj, HSAttribute* attr);
bool    ATTR_DENY_ALL   (HSObject* obj, HSAttribute* attr);
#define ATTR_READ_ONLY  NULL

HSObject* hsobject_find_child(HSObject* obj, char* name);

int list_objects_command(int argc, char* argv[], GString* output);
int print_object_tree_command(int argc, char* argv[], GString* output);

void hsobject_complete_children(HSObject* obj, char* needle, GString* output);

#endif

