#ifndef __HLWM_CHILD_H_
#define __HLWM_CHILD_H_

#include "object.h"

template<typename T>
class Child_ {
    // implement a pointer to a child object
    // in the object tree.
    // if this is assigned a new value, then the child object
    // in the owner is updated automatically
public:
    // owner is the 'parent' object
    // 'name' is the name of the child pointer
    Child_(Object& owner_, std::string name_)
        : owner(owner_)
        , name(name_)
    { }
    void operator=(T* new_value) {
        if (new_value == pointer) {
            // nothing to do
            return;
        }
        pointer = new_value;
        if (pointer) {
            owner.addChild(pointer, name);
        } else {
            owner.removeChild(name);
        }
    }
    T* operator()() {
        return pointer;
    }
    T* operator->() {
        return pointer;
    }
private:
    Object& owner;
    std::string name;
    T* pointer = nullptr;
};

#endif
