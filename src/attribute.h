#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

#include <string>
#include <vector>

#include "entity.h"

class Object;
class Signal;
class Completion;

/* Attributes.
 * Attributes are members of Objects that expose internal state variables to the
 * object tree accessible through the 'attr' command.
 * The class Attribute is abstract and is used to form a common base.
 *
 * Attribute_<T> are regular (static) attributes.
 * They provide static methods for conversion from/to string (parse() and str()).
 * A validator can be specified that performs checks before accepting external
 * input. Error messages produced by the validator are propagated back to the user.
 * The validator is not called when the attribute is set internally.
 * Static attributes are read-only by default and need to set writeable either
 * explicitely or by setting a validator.
 * Static attributes provide a signal "changed" that emits whenever the attribute
 * is altered (internally or externally).
 * Likewise, they are by-default hookable. For some attributes a hook does not make
 * sense, so it can be disabled on an individual basis.
 *
 * DynAttribute_<T> are shallow attributes and not physically present.
 * A common reason for having a dynamic attribute is for backwards-compatibility
 * or to provide some kind of magic in an attribute.
 * However, for allowing triggers by the user, an Action should be used instead.
 * Actions are designated properly and can accept multiple arguments.
 * The can be read-only using a getter callback, or rw with an additional
 * setter callback. Validation is done within the setter.
 * Dynamic attributes do not provide a changed signal. They could do it easily in
 * theory, however it would most-probably be a bad idea to attach to such a signal
 * instead of to the underlying mechanics.
 * Note that for similar reasons, dynamic attributes are not hookable.
 */

class Attribute : public Entity, public HasDocumentation {

public:
    Attribute(const std::string &name,
              bool writeable)
        : Entity(name), writeable_(writeable) {}
    ~Attribute() override = default;

    // set the owner after object creation (when pointer is available)
    void setOwner(Object *owner) { owner_ = owner; }
    // make this attribute writeable (default is typically read-only)
    void setWriteable(bool writeable = true) { writeable_ = writeable; }
    // change if attribute can be expected to trigger hooks (rarely used)
    void setHookable(bool hookable) { hookable_ = hookable; }

    Type type() override { return Type::ATTRIBUTE; }

    bool writeable() const { return writeable_; }
    bool hookable() const { return hookable_; }
    virtual Signal& changed() = 0;

    virtual std::string str() { return {}; }
    virtual std::string change(const std::string &payload) = 0;
    //! suggestions for a new value of the attribute
    virtual void complete(Completion& complete) = 0;

    // find the current value of the attribute in the given range and then
    // assign the succeeding value to the attribute (wth wrap around)
    std::string cycleValue(std::vector<std::string>::const_iterator begin,
                           std::vector<std::string>::const_iterator end);

    //! if the attribute has a default value, reset it, otherwise return false
    virtual bool resetValue() { return false; }

    void detachFromOwner();

protected:
    Object *owner_ = nullptr;

    bool writeable_ = false, hookable_ = true;
};

class Action : public Entity {
public:
    Action() = default;
    Action(const std::string &name)
        : Entity(name) {}
    void setOwner(Object *owner) { owner_ = owner; }

    Type type() override { return Type::ACTION; }

private:
    Object *owner_ = {};
};


#endif // ATTRIBUTE_H
