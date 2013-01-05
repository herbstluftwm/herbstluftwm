/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HS_OBJECT_H_
#define __HS_OBJECT_H_

#include <stdbool.h>
#include <glib.h>

typedef struct {
    //struct HSAttribute* attributes;
    //int                 attribute_count;
    GList*              children; // list of HSObjectChild
} HSObject;

void object_tree_init();
void object_tree_destroy();

HSObject* hsobject_root();

bool hsobject_init(HSObject* obj);
void hsobject_free(HSObject* obj);
void hsobject_link(HSObject* parent, HSObject* child, char* name);
void hsobject_unlink(HSObject* parent, HSObject* child);

HSObject* hsobject_find_child(HSObject* obj, char* name);

int list_objects_command(int argc, char* argv[], GString* output);

void hsobject_complete_children(HSObject* obj, char* needle, GString* output);

#endif

