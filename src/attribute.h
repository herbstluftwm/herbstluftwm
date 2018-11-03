#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

#include "x11-types.h"
#include "entity.h"

#include <string>
#include <vector>
#include <functional>

class Object;
class Signal;

class Attribute : public Entity {

public:
    Attribute(const std::string &name,
              bool writeable)
        : Entity(name), owner_(nullptr)
        , writeable_(writeable), hookable_(true) {}
    virtual ~Attribute() {}

    // set the owner after object creation (when pointer is available)
    void setOwner(Object *owner) { owner_ = owner; }
    // make this attribute writeable (default is typically read-only)
    void setWriteable(bool writeable = true) { writeable_ = writeable; }
    // change if attribute can be expected to trigger hooks (rarely used)
    void setHookable(bool hookable) { hookable_ = hookable; }

    virtual Type type() { return Type::ATTRIBUTE; }

    bool writeable() const { return writeable_; }
    bool hookable() const { return hookable_; }
    virtual Signal& changed() = 0;

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
