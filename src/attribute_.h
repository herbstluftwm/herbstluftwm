#ifndef ATTRIBUTE__H
#define ATTRIBUTE__H

#include "attribute.h"
#include "object.h"
#include "x11-types.h" // for hl::Color
#include <functional>
#include <stdexcept>


template<typename T>
class Attribute_ : public Attribute {
public:
    // default constructor
    //Attribute_(const std::string &name, ValueValidator onChange)
    //    : Attribute(name, writeable) {}
    // a read-only attribute

    // Attribute_()
    //    __attribute__((deprecated("You have to initialize an Attribute explicitly"))) {};

    Attribute_(const std::string &name, const T &payload)
        : Attribute(name, false)
        , payload_ (payload)
    {
    }
    Attribute_(const std::string &name, ValueValidator onChange, const T &payload)
        : Attribute(name, true)
        , m_onChange(onChange)
        , payload_ (payload)
    {
    }

    ValueValidator m_onChange;
    void setOnChange(ValueValidator vv) { m_onChange = vv; }

    inline Type type();

    // accessors only to be used by owner!
    operator T() { return payload_; }
    operator const T() const { return payload_; }
    std::string str() { return std::to_string(payload_); }
    // operator= is only used by the owner
    void operator=(const T &payload) {
        payload_ = payload;
        notifyHooks();
    }
    // this assigns and checks the validity of the data
    // in case of further constraints.
    // (i.e. a tag name should not clash with any other tag name)

public:

    bool operator==(const T &payload) {
        return payload_ == payload;
    }
    bool operator!=(const T &payload) {
        return payload_ != payload;
    }

    /** parse a text into the right type,
     * possibly raising a std::invalid_argument exception. The string can be
     * a specification in relative to 'reference', e.g. "toggle" for booleans.
     */
    static T parse(const std::string& source, T const* reference);

    std::string change(const std::string &payload_str) {
        try {
            T new_payload = parse(payload_str, &payload_);
            T old_payload = payload_;
            payload_ = new_payload;
            std::string error_message = this->writeable() ? (m_onChange)() : "";
            if (error_message == "") {
                // no error -> keep value
                notifyHooks();
                return {};
            } else {
                // error -> restore value
                payload_ = old_payload;
                return error_message;
            }
        } catch (std::invalid_argument const& e) {
            return std::string("invalid argument: ") + e.what();
        } catch (std::out_of_range const& e) {
            return std::string("out of range: ") + e.what();
        }
    }

    const T& operator*() const {
        return payload_;
    };
    const T* operator->() const {
        return &payload_;
    };
    const T& operator()() const {
        return payload_;
    };

protected:
    void notifyHooks() {
        if (owner_) {
            owner_->notifyHooks(HookEvent::ATTRIBUTE_CHANGED, name_);
        }
    }

    T payload_;
};

/** Integer **/

template<>
inline Type Attribute_<int>::type() { return Type::ATTRIBUTE_INT; }

template<>
inline int Attribute_<int>::parse(const std::string &payload, int const*) {
    return std::stoi(payload);
}

/** Unsigned **/

template<>
inline Type Attribute_<unsigned long>::type() { return Type::ATTRIBUTE_ULONG; }

template<>
inline unsigned long Attribute_<unsigned long>::parse(const std::string &payload, unsigned long const*) {
    return std::stoul(payload);
}

/** Boolean **/

template<>
inline Type Attribute_<bool>::type() { return Type::ATTRIBUTE_BOOL; }

template<>
inline std::string Attribute_<bool>::str() {
    return { payload_ ? "true" : "false" };
}

template<>
inline bool Attribute_<bool>::parse(const std::string &payload, bool const* ref) {
    if (payload == "off" || payload == "false")
        return false;
    else if (payload == "on" || payload == "true")
        return true;
    else if (payload == "toggle" && ref != NULL)
        return ! ref;
    else throw std::invalid_argument(
            (ref != NULL)
            ? "only on/off/true/false/toggle are valid booleans"
            : "only on/off/true/false are valid booleans");
}

/** STRING **/

template<>
inline Type Attribute_<std::string>::type() { return Type::ATTRIBUTE_STRING; }

template<>
inline std::string Attribute_<std::string>::str() { return payload_; }

template<>
inline std::string Attribute_<std::string>::parse(const std::string &payload, std::string const* ref) {
    return payload;
}

/** COLOR **/

template<>
inline Type Attribute_<Color>::type() { return Type::ATTRIBUTE_COLOR; }

template<>
inline std::string Attribute_<Color>::str() { return payload_.str(); }

template<>
inline Color Attribute_<Color>::parse(const std::string &payload, Color const* ref) {
    Color new_color;
    std::string msg = Color::fromStr(payload, new_color);
    if (msg != "") throw std::invalid_argument(msg);
    return new_color;
}

template<typename T>
class DynAttribute_ : public Attribute_<T> {
public:
    // each time a dynamic attribute is read, the getter_ is called in order to
    // get the actual value
    DynAttribute_(const std::string &name, std::function<T()> getter_)
        : Attribute_<T>(name, {})
        , setter()
        , getter(getter_)
    {
        Attribute_<T>::hookable_ = false;
    }

    DynAttribute_(const std::string &name, std::function<std::string(T)> setter_, std::function<T()> getter_)
        : Attribute_<T>(name, ([this]() {
                return this->setter(this->lastPayload());
            }), {})
        , setter(setter_)
        , getter(getter_)
    {
        Attribute_<T>::hookable_ = false;
    }
    std::string str() {
        Attribute_<T>::payload_ = getter();
        return Attribute_<T>::str();
    }
private:
    T lastPayload() {
        return Attribute_<T>::payload_;
    };
    std::function<std::string(T)> setter;
    std::function<T()> getter;
};

#endif // ATTRIBUTE__H

