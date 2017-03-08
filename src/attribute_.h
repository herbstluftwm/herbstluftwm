#ifndef ATTRIBUTE__H
#define ATTRIBUTE__H

#include "attribute.h"
#include "object.h"
#include "x11-types.h" // for hl::Color
#include <functional>


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
    std::string assignByUser(const T &payload) {
        T old_payload = payload_;
        payload_ = payload;
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
    }
    bool operator==(const T &payload) {
        return payload_ == payload;
    }
    bool operator!=(const T &payload) {
        return payload_ != payload;
    }
    std::string change(const std::string &payload);

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
inline std::string Attribute_<int>::change(const std::string &payload) {
    try {
        return assignByUser(std::stoi(payload));
    } catch (std::exception const& e) {
        return "not a valid integer";
    }
}

/** Unsigned **/

template<>
inline Type Attribute_<unsigned long>::type() { return Type::ATTRIBUTE_ULONG; }

template<>
inline std::string Attribute_<unsigned long>::change(const std::string &payload) {
    return assignByUser(std::stoul(payload));
}

/** Boolean **/

template<>
inline Type Attribute_<bool>::type() { return Type::ATTRIBUTE_BOOL; }

template<>
inline std::string Attribute_<bool>::str() {
    return { payload_ ? "true" : "false" };
}

template<>
inline std::string Attribute_<bool>::change(const std::string &payload) {
    bool new_data = payload_;
    if (payload == "off" || payload == "false")
        new_data = false;
    else if (payload == "on" || payload == "true")
        new_data = true;
    else if (payload == "toggle")
        new_data = !new_data;
    else
        return "only on/off/true/false/toggle are valid booleans";
    return assignByUser(new_data);
}

/** STRING **/

template<>
inline Type Attribute_<std::string>::type() { return Type::ATTRIBUTE_STRING; }

template<>
inline std::string Attribute_<std::string>::str() { return payload_; }

template<>
inline std::string Attribute_<std::string>::change(const std::string &payload) {
    return assignByUser(payload);
}

/** COLOR **/

template<>
inline Type Attribute_<Color>::type() { return Type::ATTRIBUTE_COLOR; }

template<>
inline std::string Attribute_<Color>::str() { return payload_.str(); }

template<>
inline std::string Attribute_<Color>::change(const std::string &payload) {
    Color new_color;
    std::string msg = Color::fromStr(payload, new_color);
    if (msg != "") return msg;
    return assignByUser(new_color);
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

