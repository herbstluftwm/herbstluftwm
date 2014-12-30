/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HS_OBJECT_H_
#define __HS_OBJECT_H_

#include "entity.h"
#include "attribute.h"

#include <unordered_map>

namespace herbstluft {

class Object : public Entity {

public:
    Object(const std::string &name) : Entity(name) {}
    virtual ~Object() {}

    // store pointer to ourselves, initialize all attributes, actions
    virtual void init(std::weak_ptr<Object> self) { self_ = self; };

    virtual Type type() { return Type::OBJECT; }

    virtual bool readable(const std::string &attr) const;
    virtual std::string read(const std::string &attr) const;
    virtual bool writeable(const std::string &attr) const;
    virtual void write(const std::string &attr, const std::string &value);
    virtual void trigger(const std::string &action, const std::string &args);

protected:
    // initialize an attribute (typically used by init())
    virtual void wireAttributes(std::vector<Attribute*> attrs);
    virtual void wireActions(std::vector<Action*> actions);

    std::unordered_map<std::string, std::shared_ptr<Object>> children_;
    std::unordered_map<std::string, Attribute*> attribs_;
    std::unordered_map<std::string, Action*> actions_;

    std::weak_ptr<Object> self_;
};

}

#include <stdbool.h>
#include "glib-backports.h"
#include "x11-types.h"

#define OBJECT_PATH_SEPARATOR '.'
#define USER_ATTRIBUTE_PREFIX "my_"
#define TMP_OBJECT_PATH "tmp"

class HSAttribute;

typedef struct HSObject {
    HSAttribute* attributes;
    size_t              attribute_count;
    GList*              children; // list of HSObjectChild
    void*               data;     // user data pointer
} HSObject;

// data pointer is the data pointer of the attribute
// if this is NULL it is the data-pointer of the object
typedef void (*HSAttributeCustom)(void* data, GString* output);
typedef int (*HSAttributeCustomInt)(void* data);
typedef GString* (*HSAttributeChangeCustom)(HSAttribute* attr, const char* new_value);

typedef union HSAttributePointer {
    bool*       b;
    int*        i;
    unsigned int* u;
    GString**   str;
    herbstluft::Color*    color;
    HSAttributeCustom custom;
    HSAttributeCustomInt custom_int;
    HSAttributePointer(bool* b) : b(b) { };
    HSAttributePointer(int* x) : i(x) { };
    HSAttributePointer(unsigned int* x) : u(x) { };
    HSAttributePointer(GString** x) : str(x) { };
    HSAttributePointer(herbstluft::Color* x) : color(x) { };
    HSAttributePointer(HSAttributeCustom x) : custom(x) { };
    HSAttributePointer(HSAttributeCustomInt x) : custom_int(x) { };
} HSAttributePointer;

typedef union HSAttributeValue {
    bool        b;
    int         i;
    unsigned int u;
    GString*    str;
    herbstluft::Color     color;
} HSAttributeValue;

typedef GString* (*HSAttrCallback)(HSAttribute* attr);

enum  HSAttributeType {
        HSATTR_TYPE_BOOL,
        HSATTR_TYPE_UINT,
        HSATTR_TYPE_INT,
        HSATTR_TYPE_COLOR,
        HSATTR_TYPE_STRING,
        HSATTR_TYPE_CUSTOM,
        HSATTR_TYPE_CUSTOM_INT,
};

class HSAttribute {
public:
    HSObject* object;           /* the object this attribute is in */
    HSAttributeType type;       /* the datatype */
    const char*  name;          /* name as it is displayed to the user */
    HSAttributePointer value;
    GString*           unparsed_value;
    /** if type is not custom:
     * on_change is called after the user changes the value. If this
     * function returns NULL, the value is accepted. If this function returns
     * some error message, the old value is restored automatically and the
     * message first is displayed to the user and then freed.
     *
     * if type is custom:
     * on_change will never be called. Instead, change_custom is called with
     * the new value requested by the user. If the pointer is NULL, it is
     * treaten read-only
     * */
    HSAttrCallback on_change;
    HSAttributeChangeCustom change_custom;
    bool user_attribute;    /* if this attribute was added by the user */
    bool           always_callback; /* call on_change/change_custom on earch write,
                                     * even if the value did not change */
    /* save the user_data at a constant position that is not shifted when
     * realloc'ing the HSAttribute */
    HSAttributeValue* user_data; /* data needed for user attributes */
    void* data; /* data which is passed to value.custom and value.custom_int */

#define ATTRIBUTE(N, V, CHANGE) HSAttribute(N, &(V), CHANGE)
#define ATTRIBUTE_STRING(N, V, CHANGE) ATTRIBUTE(N, V, CHANGE)
#define ATTRIBUTE_INT(N, V, CHANGE) ATTRIBUTE(N, V, CHANGE)
#define ATTRIBUTE_UINT(N, V, CHANGE) ATTRIBUTE(N, V, CHANGE)
#define ATTRIBUTE_BOOL(N, V, CHANGE) ATTRIBUTE(N, V, CHANGE)
#define ATTRIBUTE_COLOR(N, V, CHANGE) ATTRIBUTE(N, V, CHANGE)
#define ATTRIBUTE_CUSTOM(N, R, W) HSAttribute(N, R, W)
#define ATTRIBUTE_CUSTOM_INT(N, R, W) HSAttribute(N, R, W)

    // simple attribute
    #define HSAttributeSimpleConstructor(ETYPE, CTYPE)\
        HSAttribute(const char* name, CTYPE* v, HSAttrCallback on_change) \
            : object(NULL), type(ETYPE), name(name), value(v), \
              /* all the other attributes: */ \
              unparsed_value(NULL), on_change(on_change), change_custom(NULL), \
              user_attribute(false), always_callback(false), \
              user_data(NULL), data(NULL)
    HSAttributeSimpleConstructor(HSATTR_TYPE_BOOL, bool) {};
    HSAttributeSimpleConstructor(HSATTR_TYPE_INT, int) {};
    HSAttributeSimpleConstructor(HSATTR_TYPE_UINT, unsigned int) {};
    HSAttributeSimpleConstructor(HSATTR_TYPE_COLOR, herbstluft::Color) {
        unparsed_value = g_string_new("");
    };
    HSAttributeSimpleConstructor(HSATTR_TYPE_STRING, GString*) {};
    HSAttribute(const char* name, HSAttributeCustom custom, HSAttributeChangeCustom on_change)
        : object(NULL), type(HSATTR_TYPE_CUSTOM), name(name), value(custom),
          // all the other attributes:
          unparsed_value(NULL), on_change(NULL), change_custom(on_change),
          user_attribute(false), always_callback(false),
          user_data(NULL), data(NULL) {};
    HSAttribute(const char* name, HSAttributeCustomInt custom, HSAttributeChangeCustom on_change)
        : object(NULL), type(HSATTR_TYPE_CUSTOM_INT), name(name), value(custom),
          // all the other attributes:
          unparsed_value(NULL), on_change(NULL), change_custom(on_change),
          user_attribute(false), always_callback(false),
          user_data(NULL), data(NULL) {};

    static HSAttribute LAST() {
        return HSAttribute();
    };
private:
    HSAttribute() : value((int*)NULL) {
        name = NULL;
    };
};

#define ATTRIBUTE_LAST HSAttribute::LAST()

void object_tree_init();
void object_tree_destroy();

HSObject* hsobject_root();

bool hsobject_init(HSObject* obj);
void hsobject_free(HSObject* obj);
HSObject* hsobject_create();
HSObject* hsobject_create_and_link(HSObject* parent, const char* name);
void hsobject_destroy(HSObject* obj);
void hsobject_link(HSObject* parent, HSObject* child, const char* name);
void hsobject_unlink(HSObject* parent, HSObject* child);
void hsobject_unlink_by_name(HSObject* parent, const char* name);
void hsobject_link_rename(HSObject* parent, char* oldname, char* newname);
void hsobject_link_rename_object(HSObject* parent, HSObject* child, char* newname);
void hsobject_unlink_and_destroy(HSObject* parent, HSObject* child);

HSObject* hsobject_by_path(char* path);
HSObject* hsobject_parse_path(const char* path, const char** unparsable);
HSObject* hsobject_parse_path_verbose(const char* path, const char** unparsable, GString* output);

HSAttribute* hsattribute_parse_path(const char* path);
HSAttribute* hsattribute_parse_path_verbose(const char* path, GString* output);

void hsobject_set_attributes(HSObject* obj, HSAttribute* attributes);

GString* ATTR_ACCEPT_ALL(HSAttribute* attr);
#define ATTR_READ_ONLY  NULL

HSObject* hsobject_find_child(HSObject* obj, const char* name);
HSAttribute* hsobject_find_attribute(HSObject* obj, const char* name);
void hsobject_set_attributes_always_callback(HSObject* obj);

char hsattribute_type_indicator(int type);

int attr_command(int argc, char* argv[], GString* output);
int print_object_tree_command(int argc, char* argv[], GString* output);
int hsattribute_get_command(int argc, const char* argv[], GString* output);
int hsattribute_set_command(int argc, char* argv[], GString* output);
bool hsattribute_is_read_only(HSAttribute* attr);
int hsattribute_assign(HSAttribute* attr, const char* new_value_str, GString* output);
void hsattribute_append_to_string(HSAttribute* attribute, GString* output);
GString* hsattribute_to_string(HSAttribute* attribute);

void hsobject_complete_children(HSObject* obj, const char* needle, const char* prefix,
                                GString* output);
void hsobject_complete_attributes(HSObject* obj, bool user_only,
                                  const char* needle, const char* prefix,
                                  GString* output);
int substitute_command(int argc, char* argv[], GString* output);
int sprintf_command(int argc, char* argv[], GString* output);
int compare_command(int argc, char* argv[], GString* output);

int userattribute_command(int argc, char* argv[], GString* output);
int userattribute_remove_command(int argc, char* argv[], GString* output);
HSAttribute* hsattribute_create(HSObject* obj, const char* name, char* type_str,
                                GString* output);
bool userattribute_remove(HSAttribute* attr);
int tmpattribute_command(int argc, char* argv[], GString* output);

#endif

