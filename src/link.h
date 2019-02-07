#ifndef HLWM_LINK_H_
#define HLWM_LINK_H_

#include "object.h"

/*! A pointer to another object in the object tree. if this is
 * assigned a new value, then the child object in the owner is updated
 * automatically.
 */
template<typename T>
class Link_ {
public:
    // 'name' is the name of the child pointer
    Link_(Object& parent, std::string name)
        : parent_(parent)
        , name_(name)
    { }
    void operator=(T* new_value) {
        if (new_value == pointer) {
            // nothing to do
            return;
        }
        pointer = new_value;
        if (pointer) {
            parent_.addChild(pointer, name_);
        } else {
            parent_.removeChild(name_);
        }
    }
    T* operator()() {
        return pointer;
    }
    T* operator->() {
        return pointer;
    }
private:
    Object& parent_;
    std::string name_;
    T* pointer = nullptr;
};

#endif
