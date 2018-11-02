#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

#include "x11-types.h"
#include "entity.h"

#include <string>
#include <vector>
#include <functional>

// a member function of an object that validates a new attribute value
// if the attribute value is valid, then the ValueValidator has to return the
// empty string.
// if the attribute value is invalid, then ValueValidator has to return an
// error message. In this case, the original value will be restored and the
// error message is escalated to the user.
// if the ValueValidator is itself just NULL, then any value is rejected, i.e.
// the attribute is read-only.
//typedef std::string (Object::*ValueValidator)();
typedef std::function<std::string()> ValueValidator;

// binds away the first parameter with *this
#define AT_THIS(X) ([this]() { return this->X(); })
std::string ACCEPT_ALL();

class Object;

class Attribute : public Entity {
public:
    Attribute() {}
    Attribute(const std::string &name,
              bool writeable)
        : Entity(name), owner_(nullptr)
        , writeable_(writeable), hookable_(true) {}
    virtual ~Attribute() {}

    // set the owner after object creation (when pointer is available)
    void setOwner(Object *owner) { owner_ = owner; }
    // change if attribute can be expected to trigger hooks (rarely used)
    void setHookable(bool hookable) { hookable_ = hookable; }

    virtual void setOnChange(ValueValidator) {}

    virtual Type type() { return Type::ATTRIBUTE; }

    bool writeable() const { return writeable_; }
    bool hookable() const { return hookable_; }

    virtual std::string str() { return {}; }
    virtual std::string change(const std::string &payload) = 0;

    // find the current value of the attribute in the given range and then
    // assign the succeeding value to the attribute (wth wrap around)
    std::string cycleValue(std::vector<std::string>::const_iterator begin,
                           std::vector<std::string>::const_iterator end);

    void detachFromOwner();

protected:
    Object *owner_;
    bool writeable_, hookable_;
};

class Action : public Entity {
public:
    Action() {}
    Action(const std::string &name)
        : Entity(name) {}
    void setOwner(Object *owner) { owner_ = owner; }

    Type type() { return Type::ACTION; }

private:
    Object *owner_;
};


#endif // ATTRIBUTE_H
