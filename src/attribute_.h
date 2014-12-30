#ifndef ATTRIBUTE__H
#define ATTRIBUTE__H

#include "attribute.h" // just for the editor
#include "x11-types.h" // for hl::Color

namespace herbstluft {

template<typename T>
class Attribute_ : public Attribute {
public:
    // default constructor
    Attribute_() {}
    Attribute_(const std::string &name,
               bool readable, bool writeable)
        : Attribute(name, readable, writeable) {}
    Attribute_(const std::string &name,
               bool readable, bool writeable, const T &payload)
        : Attribute_(name, readable, writeable) { payload_ = payload; }

    inline Type type();

    // accessors only to be used by owner!
    operator T() { return payload_; }
    std::string str() { return std::to_string(payload_); }
    void operator=(const T &payload) { payload_ = payload; }
    void change(const std::string &payload);

private:
    T payload_;
};

/** Integer **/

template<>
inline Type Attribute_<int>::type() { return Type::ATTRIBUTE_INT; }

template<>
inline void Attribute_<int>::change(const std::string &payload) {
    payload_= std::stoi(payload);
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
}

/** STRING **/

template<>
inline Type Attribute_<std::string>::type() { return Type::ATTRIBUTE_STRING; }

template<>
inline std::string Attribute_<std::string>::str() { return payload_; }

template<>
inline void Attribute_<std::string>::change(const std::string &payload) {
    payload_ = payload;
}

/** COLOR **/

template<>
inline Type Attribute_<Color>::type() { return Type::ATTRIBUTE_COLOR; }

template<>
inline std::string Attribute_<Color>::str() { return payload_.str(); }

template<>
inline void Attribute_<Color>::change(const std::string &payload) {
    payload_ = Color::fromStr(payload.c_str());
}

}

#endif // ATTRIBUTE__H

