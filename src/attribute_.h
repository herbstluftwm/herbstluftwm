#ifndef ATTRIBUTE__H
#define ATTRIBUTE__H

#include "attribute.h"
#include "object.h"
#include "signal.h"
#include "x11-types.h" // for Color
#include <functional>
#include <stdexcept>

template<typename T>
class Attribute_ : public Attribute {
public:
    // function that validates a new attribute value against the current state
    // if the attribute value is valid, returns the empty string.
    // if the attribute value is invalid, returns an error message to be
    // escalated to the user.
    using Validator = std::function<std::string(T)>;

    Attribute_(const std::string &name, const T &payload)
        : Attribute(name, false)
        , payload_ (payload)
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

    Signal_<T>& changed() override { return changed_; }

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
        if (!writeable()) return "attribute is read-only";
        try {
            T new_payload = Converter<T>::parse(payload_str, &payload_); // throws

            // validate, if needed
            if (validator_) {
                auto error_message = (validator_)(new_payload);
                if (error_message != "")
                    return error_message;
            }

            // set and trigger stuff
            if (new_payload != payload_)
                this->operator=(new_payload);
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
    T payload_;
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
    DynAttribute_(const std::string &name, std::function<T()> getter)
        : Attribute(name, false)
        , getter_(getter)
    {
        hookable_ = false;
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

    Type type() override { return Attribute_<T>::staticType(); }

    Signal_<T>& changed() override {
        throw new std::logic_error(
                    "No change signalling on dynamic attributes.");
    }

    std::string str() override {
        return Converter<T>::str(getter_());
    }

    std::string change(const std::string &payload_str) override {
        if (!writeable()) return "attribute is read-only";
        try {
            T new_payload = Converter<T>::parse(payload_str, nullptr); // throws
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

