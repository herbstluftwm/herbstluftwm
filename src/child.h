#ifndef __HLWM_CHILD_H_
#define __HLWM_CHILD_H_

#include <memory>

#include "object.h"

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

/*! A member variable that is exported to the object tree.
 * This is a more direct variation of the Child_<> template,
 * in the sense that here is no pointer indirection in between.
 * You can use a member variable of type ChildMember_<T> as you
 * would use a member of type T.
 */
template<typename T>
class ChildMember_ : public T {
public:
    // owner is the 'parent' object
    // 'name' is the name of the child pointer
    template<typename... Args>
    ChildMember_(Object& owner_, const std::string& name_, Args&&... args)
        : T(std::forward<Args>(args)...)
        , owner(owner_)
        , name(name_)
    {
        owner.addChild(static_cast<T*>(this), name_);
    }

private:
    Object& owner;
    std::string name;
};

template<typename T>
class DynChild_ {
public:
    // A dynamic child is a callback function that dynamically
    // returns an object of a certain type.
    template <typename Owner>
    DynChild_(Owner& owner, const std::string &name, T* (Owner::*getter)())
    {
        owner.addDynamicChild( [&owner,getter] { return (owner.*getter)(); }, name);
    }
};

#endif
