#ifndef HLWM_LINK_H_
#define HLWM_LINK_H_

#include "entity.h"
#include "object.h"
#include "signal.h"

/*! A pointer to another object in the object tree. if this is
 * assigned a new value, then the child object in the owner is updated
 * automatically.
 */
template<typename T>
class Link_ : public ChildEntry {
public:
    // 'name' is the name of the child pointer
    Link_(Object& parent, std::string name)
        : ChildEntry(parent, name)
    { }
    void operator=(T* new_value) {
        if (new_value == pointer) {
            // nothing to do
            return;
        }
        pointer = new_value;
        if (pointer) {
            owner_.addChild(pointer, name_);
        } else {
            owner_.removeChild(name_);
        }
        changed_.emit(new_value);
    }
    Signal_<T*>& changed() { return changed_; }
    T* operator()() {
        return pointer;
    }
    T* operator->() {
        return pointer;
    }
private:
    Signal_<T*> changed_;
    T* pointer = nullptr;
};

#endif
