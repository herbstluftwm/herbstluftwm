#ifndef __HERBSTLUFT_HOOK_H_
#define __HERBSTLUFT_HOOK_H_

#include <string>
#include <vector>

class HSTag;
class Object;

// a Hook monitors a given object, i.e. gets callbacks
// each time a the object changes its attributes or its children.
class Hook {
public:
    Hook() = default;
    virtual ~Hook() = default;
    // this is called after a child has been added
    virtual void childAdded(Object* parent, std::string child_name) {}
    // this is called immediately before a child is removed
    virtual void childRemoved(Object* parent, std::string child_name) {}
    // this is called after an attribute value has changed
    virtual void attributeChanged(Object* sender, std::string attribute_name) {}
};

void hook_emit(std::vector<std::string> args);
void emit_tag_changed(HSTag* tag, int monitor);
void hook_emit_list(const char* name, ...);

#endif

