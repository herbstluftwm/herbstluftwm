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
        pointer_ = std::unique_ptr<T>(new T(std::forward<Args>(args)...));
        owner.addChild(pointer_.get(), name);
    }

    void reset() {
        pointer_.reset();
    }

    T* operator()() {
        return pointer_.get();
    }
    T* operator->() {
        return pointer_.get();
    }
private:
    Object& owner;
    std::string name;
    std::unique_ptr<T> pointer_ = nullptr;
};

#endif
