#ifndef ATTRIBUTE__H
#define ATTRIBUTE__H

#include "attribute.h"
#include "object.h"
#include "x11-types.h" // for hl::Color

namespace herbstluft {

// a member function of an object that validates a new attribute value
// if the attribute value is valid, then the ValueValidator has to return the
// empty string.
// if the attribute value is invalid, then ValueValidator has to return an
// error message. In this case, the original value will be restored and the
// error message is escalated to the user.
// if the ValueValidator is itself just NULL, then any value is rejected, i.e.
// the attribute is read-only.
typedef std::string (Object::*ValueValidator)();

#define READ_ONLY ((ValueValidator)NULL)

template<typename T>
class Attribute_ : public Attribute {
public:
    // default constructor
    //Attribute_(const std::string &name, ValueValidator onChange)
    //    : Attribute(name, writeable) {}
    Attribute_(const std::string &name, ValueValidator onChange, const T &payload)
        : Attribute(name, onChange != NULL)
        , m_onChange (onChange)
        , payload_ (payload)
    {
    }

    inline Type type();
    ValueValidator m_onChange;

    // accessors only to be used by owner!
    operator T() { return payload_; }
    operator const T() const { return payload_; }
    std::string str() { return std::to_string(payload_); }
    void operator=(const T &payload) {
        T old_payload = payload_;
        payload_ = payload;
        std::string error_message = m_onChange ? ((*owner_).*m_onChange)() : "";
        if (error_message != "") {
            // no error -> keep value
            notifyHooks();
        } else {
            // error -> restore value
            payload_ = old_payload;
            // FIXME: also return the error message
        }
    }
    bool operator==(const T &payload) {
        return payload_ == payload;
    }
    bool operator!=(const T &payload) {
        return payload_ != payload;
    }
    void change(const std::string &payload);

    const T& operator*() const {
        return payload_;
    };
    const T* operator->() const {
        return &payload_;
    };
    const T& operator()() const {
        return payload_;
    };

private:
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
inline void Attribute_<int>::change(const std::string &payload) {
    payload_= std::stoi(payload);
    notifyHooks();
}

/** Unsigned **/

template<>
inline Type Attribute_<unsigned long>::type() { return Type::ATTRIBUTE_ULONG; }

template<>
inline void Attribute_<unsigned long>::change(const std::string &payload) {
    payload_= std::stoul(payload);
    notifyHooks();
}

/** Boolean **/

template<>
inline Type Attribute_<bool>::type() { return Type::ATTRIBUTE_BOOL; }

template<>
inline std::string Attribute_<bool>::str() {
    return { payload_ ? "true" : "false" };
}

template<>
inline void Attribute_<bool>::change(const std::string &payload) {
    if (payload == "off" || payload == "false")
        payload_ = false;
    if (payload == "on" || payload == "true")
        payload_ = true;
    if (payload == "toggle")
        payload_ = !payload_;
    // TODO: throw if string could not be parsed
    notifyHooks();
}

/** STRING **/

template<>
inline Type Attribute_<std::string>::type() { return Type::ATTRIBUTE_STRING; }

template<>
inline std::string Attribute_<std::string>::str() { return payload_; }

template<>
inline void Attribute_<std::string>::change(const std::string &payload) {
    payload_ = payload;
    notifyHooks();
}

/** COLOR **/

template<>
inline Type Attribute_<Color>::type() { return Type::ATTRIBUTE_COLOR; }

template<>
inline std::string Attribute_<Color>::str() { return payload_.str(); }

template<>
inline void Attribute_<Color>::change(const std::string &payload) {
    payload_ = Color::fromStr(payload);
    notifyHooks();
}

}

#endif // ATTRIBUTE__H

