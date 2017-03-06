/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_HOOK_H_
#define __HERBSTLUFT_HOOK_H_

#include <memory>
#include <vector>
#include "x11-types.h"
class Object;

// a Hook monitors a given object, i.e. gets callbacks
// each time a the object changes its attributes or its children.
class Hook {
public:
    Hook();
    virtual ~Hook() {};
    // this is called after a child has been added
    virtual void childAdded(Object* parent, std::string child_name) {};
    // this is called immediately before a child is removed
    virtual void childRemoved(Object* parent, std::string child_name) {};
    // this is called after an attribute value has changed
    virtual void attributeChanged(Object* sender, std::string attribute_name) {};
};


class HSTag;

void hook_init();
void hook_destroy();

void hook_emit(int argc, const char** argv);
void emit_tag_changed(HSTag* tag, int monitor);
void hook_emit_list(const char* name, ...);

#endif

