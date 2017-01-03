/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_HOOK_H_
#define __HERBSTLUFT_HOOK_H_

#include <memory>
#include <vector>

// a Hook monitors a given object, i.e. gets callbacks
// each time a the object changes its attributes or its children.
class Hook {
public:
    Hook();
    virtual void childAdded(std::string child_name) {};
    virtual void childRemoved(std::string child_name) {};
    virtual void attributeChanged(std::string attribute_name) {};
};


class HSTag;

void hook_init();
void hook_destroy();

void hook_emit(int argc, const char** argv);
void emit_tag_changed(HSTag* tag, int monitor);
void hook_emit_list(const char* name, ...);

#endif

