#ifndef ATTRIBUTE__H
#define ATTRIBUTE__H

#include <functional>
#include <stdexcept>

#include "attribute.h"
#include "object.h"
#include "signal.h"
#include "x11-types.h" // for Color

class Completion;

template<typename T>
class Attribute_ : public Attribute {
public:
    // function that validates a new attribute value against the current state
    // if the attribute value is valid, returns the empty string.
    // if the attribute value is invalid, returns an error message to be
    // escalated to the user.
    using Validator = std::function<std::string(T)>;

    /** The constructors for Attribute_ and also for DynAttribute_ follow
     * the a common pattern:
     *
     *   - The first argument is the owner object
     *
     *   - The second argument is the name displayed to the user
     *
     *   - The third argument the reading behaviour of the attribute:
     *     * for a plain Attribute_ this is simply the initial value
     *     * for a DynAttribute_ this is a member of the owner (or a lambda)
     *       that returns the value of the attribute.
     *
     *   - The fourth argument describes the writing behaviour of the
     *     attribute: If the fourth argument is absent, the attribute is
     *     read-only.
     *     * The fourth argument for an Attribute_ is a member of owner (or
     *       lambda) that validates a new value of the attribute and returns an
     *       error message if the new value is not acceptable for this
     *       attribute (E.g. because a name is already taken or does not
     *       respect a certain format).
     *     * The fourth argument of a DynAttribute_ is a member of owner (or
     *       lambda) that internally processes the new value (e.g. parsing) and
     *       returns an error message if the new value is not acceptable.
     */

    //! A read-only attribute of owner of type T
    Attribute_(Object* owner, const std::string &name, const T &payload)
        : Attribute(name, false)
        , validator_({})
        , payload_ (payload)
        , defaultValue_ (payload)
    {
        // the following will call Attribute::setOwner()
        // maybe this should be changed at some point,
        // e.g. when we got rid of Object::wireAttributes()
        owner->addAttribute(this);
    }

    //! A writeable attribute of owner of type T
    template <typename Owner>
    Attribute_(Owner* owner, const std::string &name, const T &payload,
              std::string(Owner::*validator)(T))
        : Attribute(name, true)
        , validator_(std::bind(validator, owner, std::placeholders::_1))
        , payload_ (payload)
        , defaultValue_ (payload)
    {
        // the following will call Attribute::setOwner()
        // maybe this should be changed at some point,
        // e.g. when we got rid of Object::wireAttributes()
        owner->addAttribute(this);
    }
    //! A writeable attribute of owner of type T
    Attribute_(Object* owner, const std::string &name, const T &payload,
              Validator validator)
        : Attribute(name, true)
        , validator_(validator)
        , payload_ (payload)
        , defaultValue_ (payload)
    {
        // the following will call Attribute::setOwner()
        // maybe this should be changed at some point,
        // e.g. when we got rid of Object::wireAttributes()
        owner->addAttribute(this);
    }

    //! Deprecated constructor, that will be removed and only remains for
    //compatibility
    Attribute_(const std::string &name, const T &payload)
        : Attribute(name, false)
        , payload_ (payload)
        , defaultValue_ (payload)
    {
    }

    // set the method called for validation of external changes
    // this implicitely makes the attribute writeable
    void setValidator(Validator v) {
        validator_ = v;
        writeable_ = true;
    }

    // delegate type() to a static methods in specializations,
    // so it can also be used by DynAttribute_<T>
    Type type() override { return Attribute_<T>::staticType(); }
    static Type staticType();

    // wrap Converter::str() for convenience
    std::string str() override { return Converter<T>::str(payload_); }

    void complete(Completion& complete) override {
        Converter<T>::complete(complete, &payload_);
    }

    //! a signal that is emitted whenever the value changes
    Signal_<T>& changed() override { return changed_; }
    //! a signal that is emitted when the user changes this attribute
    Signal_<T>& changedByUser() { return changedByUser_; }

    bool resetValue() override {
        operator=(defaultValue_);
        return true;
    }

    // accessors only to be used by owner!
    operator T() { return payload_; }
    operator const T() const { return payload_; }
    // operator= is only used by the owner and us
    void operator=(const T &payload) {
        payload_ = payload;
        notifyHooks();
        changed_.emit(payload);
    }

    bool operator==(const T &payload) {
        return payload_ == payload;
    }
    bool operator!=(const T &payload) {
        return payload_ != payload;
    }

    std::string change(const std::string &payload_str) override {
        if (!writeable()) {
            return "attribute is read-only";
        }
        try {
            T new_payload = Converter<T>::parse(payload_str, payload_); // throws

            // validate, if needed
            if (validator_) {
                auto error_message = (validator_)(new_payload);
                if (!error_message.empty()) {
                    return error_message;
                }
            }

            // set and trigger stuff
            if (new_payload != payload_) {
                this->operator=(new_payload);
                changedByUser_.emit(payload_);
            }
        } catch (std::invalid_argument const& e) {
            return std::string("invalid argument: ") + e.what();
        } catch (std::out_of_range const& e) {
            return std::string("out of range: ") + e.what();
        }
        return {}; // all good
    }

    const T& operator*() const {
        return payload_;
    }
    const T* operator->() const {
        return &payload_;
    }
    const T& operator()() const {
        return payload_;
    }

protected:
    void notifyHooks() {
        if (owner_) {
            owner_->notifyHooks(HookEvent::ATTRIBUTE_CHANGED, name_);
        }
    }

    Validator validator_;
    Signal_<T> changed_;
    Signal_<T> changedByUser_;
    T payload_;
    T defaultValue_;
};

/** Type mappings **/
template<>
inline Type Attribute_<int>::staticType() { return Type::ATTRIBUTE_INT; }
template<>
inline Type Attribute_<unsigned long>::staticType() { return Type::ATTRIBUTE_ULONG; }
template<>
inline Type Attribute_<bool>::staticType() { return Type::ATTRIBUTE_BOOL; }
template<>
inline Type Attribute_<std::string>::staticType() { return Type::ATTRIBUTE_STRING; }
template<>
inline Type Attribute_<Color>::staticType() { return Type::ATTRIBUTE_COLOR; }

template<typename T>
class DynAttribute_ : public Attribute {
public:
    // each time a dynamic attribute is read, the getter_ is called in order to
    // get the actual value
    DynAttribute_(Object* owner, const std::string &name, std::function<T()> getter)
        : Attribute(name, false)
        , getter_(getter)
    {
        hookable_ = false;
        owner->addAttribute(this);
    }

    // in this case, also write operations are delegated
    DynAttribute_(const std::string &name, std::function<T()> getter, std::function<std::string(T)> setter)
        : Attribute(name, true)
        , getter_(getter)
        , setter_(setter)
    {
        hookable_ = false;
        writeable_ = true;
    }

    void complete(Completion& completion) override {
        Converter<T>::complete(completion, nullptr);
    }

    // each time a dynamic attribute is read, the getter_ is called in order to
    // get the actual value. Here, we use C++-style member function pointers such that
    // the type checker fills the template argument 'Owner' automatically
    template <typename Owner>
    DynAttribute_(Owner* owner, const std::string &name,
                  // std::function<T(Owner*)> getter // this does not work!
                  T (Owner::*getter)()
                  )
        : Attribute(name, false)
        , getter_(std::bind(getter, owner))
    {
        hookable_ = false;
        // the following will call Attribute::setOwner()
        // maybe this should be changed at some point,
        // e.g. when we got rid of Object::wireAttributes()
        owner->addAttribute(this);
    }

    //! same as above, but only with a const getter member function
    template <typename Owner>
    DynAttribute_(Owner* owner, const std::string &name,
                  T (Owner::*getter)() const
                  )
        : Attribute(name, false)
        , getter_(std::bind(getter, owner))
    {
        hookable_ = false;
        // the following will call Attribute::setOwner()
        // maybe this should be changed at some point,
        // e.g. when we got rid of Object::wireAttributes()
        owner->addAttribute(this);
    }

    template <typename Owner>
    DynAttribute_(Owner* owner, const std::string &name,
                  T (Owner::*getter)(),
                  std::string (Owner::*setter)(T)
                  )
        : Attribute(name, false)
        , getter_(std::bind(getter, owner))
        , setter_(std::bind(setter, owner, std::placeholders::_1))
    {
        hookable_ = false;
        writeable_ = true;
        // the following will call Attribute::setOwner()
        // maybe this should be changed at some point,
        // e.g. when we got rid of Object::wireAttributes()
        owner->addAttribute(this);
    }

    Type type() override { return Attribute_<T>::staticType(); }

    Signal_<T>& changed() override {
        throw new std::logic_error(
                    "No change signalling on dynamic attributes.");
    }

    std::string str() override {
        return Converter<T>::str(getter_());
    }

    std::string change(const std::string &payload_str) override {
        if (!writeable()) {
            return "attribute is read-only";
        }
        try {
            // TODO: we _could_ use getter() here to allow relative changes.
            T new_payload = Converter<T>::parse(payload_str); // throws
            return setter_(new_payload);
        } catch (std::invalid_argument const& e) {
            return std::string("invalid argument: ") + e.what();
        } catch (std::out_of_range const& e) {
            return std::string("out of range: ") + e.what();
        }
    }

private:
    std::function<T()> getter_;
    std::function<std::string(T)> setter_;
};

#endif // ATTRIBUTE__H

