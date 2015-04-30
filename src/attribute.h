#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

#include "x11-types.h"
#include "entity.h"

namespace herbstluft {

class Object;

class Attribute : public Entity {
public:
    Attribute() {}
    Attribute(const std::string &name,
              bool writeable)
        : Entity(name), owner_(nullptr),
          writeable_(writeable), hookable_(true) {}
    virtual ~Attribute() {};

    // set the owner after object creation (when pointer is available)
    void setOwner(Object *owner) { owner_ = owner; }
    // change if attribute can be expected to trigger hooks (rarely used)
    void setHookable(bool hookable) { hookable_ = hookable; }

    virtual Type type() { return Type::ATTRIBUTE; }

    bool writeable() const { return writeable_; }
    bool hookable() const { return hookable_; }

    // TODO for both: programming error when reached, thrown an exception.
    virtual std::string str() { return {}; }
    virtual void change(const std::string &payload) {}

protected:
    Object *owner_;
    bool writeable_, hookable_;
};

/* attributes that don't hold reference a data field (no templating), but are
 * rather a shallow interface to the owner's getter and setter doing magic. */
class DynamicAttribute : public Attribute {
public:
    DynamicAttribute() {}
    DynamicAttribute(const std::string &name, Type type,
                     bool writeable, bool hookable = false)
        : Attribute(name, writeable), type_(type) { hookable_ = hookable; }

    Type type() { return type_; }

protected:
    Type type_;
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

} // close namespace before further includes!

#endif // ATTRIBUTE_H
