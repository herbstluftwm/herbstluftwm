#ifndef HERBSTLUFT_TYPES_H
#define HERBSTLUFT_TYPES_H

#include "arglist.h"
#include <set>

/* A path in the object tree */
using Path = ArgList;

/* Types for I/O with the user */
using Input = ArgList;
using Output = std::ostream&;

/* Primitive types that can be converted from/to user input/output */
template<typename T>
struct Converter {
    /** Parse a text into the right type
     * Throws std::invalid_argument or std::out_of_range
     * 'Source' may be given relative to 'relativeTo', e.g. "toggle" for booleans.
     */
    static T parse(const std::string& source, T const* relativeTo);

    /** Return a user-friendly string representation */
    static std::string str(T payload) { return std::to_string(payload); }
};

// Integers
template<>
inline int Converter<int>::parse(const std::string &payload, int const*) {
    return std::stoi(payload);
}
template<>
inline unsigned long Converter<unsigned long>::parse(const std::string &payload, unsigned long const*) {
    return std::stoul(payload);
}

// Booleans
template<>
inline std::string Converter<bool>::str(bool payload) {
    return { payload ? "true" : "false" };
}
template<>
inline bool Converter<bool>::parse(const std::string &payload, bool const* previous) {
    std::set<std::string> t = {"true", "on", "1"};
    std::set<std::string> f = {"false", "off", "0"};
    if (f.find(payload) != f.end())
        return false;
    else if (t.find(payload) != t.end())
        return true;
    else if (payload == "toggle" && previous)
        return !*previous;
    else throw std::invalid_argument(
            previous
            ? "only on/off/true/false/toggle are valid booleans"
            : "only on/off/true/false are valid booleans");
}

// Strings
template<>
inline std::string Converter<std::string>::str(std::string payload) { return payload; }
template<>
inline std::string Converter<std::string>::parse(const std::string &payload, std::string const*) {
    return payload;
}

// Note: include x11-types.h for colors

#endif
