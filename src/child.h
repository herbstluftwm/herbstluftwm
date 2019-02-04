#ifndef __HLWM_CHILD_H_
#define __HLWM_CHILD_H_

#include "object.h"
#include <memory>

/*! implement a static child object in the object tree. Static means that
 * init() is called on start up and reset() is called on shutdown.
 *
 * This is a wrapper around unique_ptr that updates the object tree
 * accordingly.
 */
template<typename T>
class Child_ {
public:
    // owner is the 'parent' object
    // 'name' is the name of the child pointer
    Child_(Object& owner_, const std::string& name_)
        : owner(owner_)
        , name(name_)
    { }

    template<typename... Args>
    void init(Args&&... args)
    {
        pointer = std::unique_ptr<T>(new T(std::forward<Args>(args)...));
        owner.addChild(pointer.get(), name);
    }

    void reset() {
        pointer.reset();
    }

    T* operator()() {
        return pointer.get();
    }
    T* operator->() {
        return pointer.get();
    }
private:
    Object& owner;
    std::string name;
    std::unique_ptr<T> pointer = nullptr;
};

#endif
