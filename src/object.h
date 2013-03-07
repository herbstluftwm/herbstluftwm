/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HS_OBJECT_H_
#define __HS_OBJECT_H_

#include <stdbool.h>
#include "glib-backports.h"

#define OBJECT_PATH_SEPARATOR '.'
#define USER_ATTRIBUTE_PREFIX "my_"

typedef struct HSObject {
    struct HSAttribute* attributes;
    size_t              attribute_count;
    GList*              children; // list of HSObjectChild
    void*               data;     // user data pointer
} HSObject;

typedef void (*HSAttributeCustom)(void* data, GString* output);
typedef int (*HSAttributeCustomInt)(void* data);

typedef struct HSAttribute {
    HSObject* object;           /* the object this attribute is in */
    enum  {
        HSATTR_TYPE_BOOL,
        HSATTR_TYPE_UINT,
        HSATTR_TYPE_INT,
        HSATTR_TYPE_STRING,
        HSATTR_TYPE_CUSTOM,
        HSATTR_TYPE_CUSTOM_INT,
    } type;                     /* the datatype */
    char*  name;                /* name as it is displayed to the user */
    union {
        bool*       b;
        int*        i;
        unsigned int* u;
        GString**   str;
        HSAttributeCustom custom;
        HSAttributeCustomInt custom_int;
    } value;
    /** if type is not custom:
     * on_change is called after the user changes the value. If this
     * function returns NULL, the value is accepted. If this function returns
     * some error message, the old value is restored automatically and the
     * message first is displayed to the user and then freed.
     *
     * if type is custom:
     * on_change will never be called, because custom are read-only for now.
     * */
    GString* (*on_change)  (struct HSAttribute* attr);
    bool user_attribute;    /* if this attribute was added by the user */
    union {                 /* data needed for user attributes */
        bool        b;
        int         i;
        unsigned int u;
        GString*   str;
    } user_data;
} HSAttribute;

#define ATTRIBUTE_BOOL(N, V, CHANGE) \
    { NULL, HSATTR_TYPE_BOOL, (N), { .b = &(V) }, (CHANGE), false }
#define ATTRIBUTE_INT(N, V, CHANGE) \
    { NULL, HSATTR_TYPE_INT, (N), { .i = &(V) }, (CHANGE), false }
#define ATTRIBUTE_UINT(N, V, CHANGE) \
    { NULL, HSATTR_TYPE_UINT, (N), { .u = &(V) }, (CHANGE), false }
#define ATTRIBUTE_STRING(N, V, CHANGE) \
    { NULL, HSATTR_TYPE_STRING, (N), { .str = &(V) }, (CHANGE), false }
#define ATTRIBUTE_CUSTOM(N, V, CHANGE) \
    { NULL, HSATTR_TYPE_CUSTOM, (N), { .custom = V }, (NULL), false }
#define ATTRIBUTE_CUSTOM_INT(N, V, CHANGE) \
    { NULL, HSATTR_TYPE_CUSTOM_INT, (N), { .custom_int = V }, (NULL), false }

#define ATTRIBUTE_LAST { .name = NULL }

void object_tree_init();
void object_tree_destroy();

HSObject* hsobject_root();

bool hsobject_init(HSObject* obj);
void hsobject_free(HSObject* obj);
HSObject* hsobject_create();
HSObject* hsobject_create_and_link(HSObject* parent, char* name);
void hsobject_destroy(HSObject* obj);
void hsobject_link(HSObject* parent, HSObject* child, char* name);
void hsobject_unlink(HSObject* parent, HSObject* child);
void hsobject_unlink_by_name(HSObject* parent, char* name);
void hsobject_link_rename(HSObject* parent, char* oldname, char* newname);
void hsobject_rename_child(HSObject* parent, HSObject* child, char* newname);
void hsobject_unlink_and_destroy(HSObject* parent, HSObject* child);

HSObject* hsobject_by_path(char* path);
HSObject* hsobject_parse_path(char* path, char** unparsable);
HSObject* hsobject_parse_path_verbose(char* path, char** unparsable, GString* output);

HSAttribute* hsattribute_parse_path_verbose(char* path, GString* output);

void hsobject_set_attributes(HSObject* obj, HSAttribute* attributes);

GString* ATTR_ACCEPT_ALL(HSAttribute* attr);
#define ATTR_READ_ONLY  NULL

HSObject* hsobject_find_child(HSObject* obj, char* name);
HSAttribute* hsobject_find_attribute(HSObject* obj, char* name);

char hsattribute_type_indicator(int type);

int attr_command(int argc, char* argv[], GString* output);
int print_object_tree_command(int argc, char* argv[], GString* output);
int hsattribute_get_command(int argc, char* argv[], GString* output);
int hsattribute_set_command(int argc, char* argv[], GString* output);
int hsattribute_assign(HSAttribute* attr, char* new_value_str, GString* output);
void hsattribute_append_to_string(HSAttribute* attribute, GString* output);
GString* hsattribute_to_string(HSAttribute* attribute);

void hsobject_complete_children(HSObject* obj, char* needle, char* prefix,
                                GString* output);
void hsobject_complete_attributes(HSObject* obj, char* needle, char* prefix,
                                GString* output);
int substitute_command(int argc, char* argv[], GString* output);
int compare_command(int argc, char* argv[], GString* output);

int userattribute_command(int argc, char* argv[], GString* output);

#endif

