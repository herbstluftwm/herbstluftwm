#pragma once

#include <map>
#include <set>
#include <stdexcept>
#include <string>

class Completion;

// only a trick such that we don't need to include completion.h here
void completeFull(Completion& complete, std::string string);

/**
 * The Converter<T> class provides static functions for all the I/O-operations
 * of the type T, used in parsing and printing of attributes and in the
 * parsing and completion of command line arguments.
 */

/* Primitive types that can be converted from/to user input/output */
template<typename T, typename = void>
struct Converter {
    /** Parse a text into the right type
     * @throws std::invalid_argument or std::out_of_range
     * @param source verbal description of a value
     */
    static T parse(const std::string& source);
    /** Parse a text into the right type
     * @brief overload that accepts a reference value to relate input string to
     * @param source may be given relative to 'relativeTo', e.g. "toggle" for booleans.
     * @param relativeTo previous/existing value
     * @throws std::invalid_argument or std::out_of_range
     */
    static T parse(const std::string& source, const T& relativeTo) {
        // note: specializations _do_ use relativeTo where appropriate
        return parse(source);
    };

    /** Return a user-friendly string representation */
    static std::string str(T payload) { return std::to_string(payload); }

    /** Give possible completion values. The completion can be relative to 'relativeTo', e.g.
     * "toggle" in booleans will be proposed only if relativeTo is present
     */
    static void complete(Completion& complete, T const* relativeTo) {
        if (relativeTo) {
            completeFull(complete, str(*relativeTo));
        }
    }
};

#define ConverterInstance(T) \
    template<> T Converter<T>::parse(const std::string& source); \
    template<> std::string Converter<T>::str(T payload); \
    template<> void Converter<T>::complete(Completion& complete, T const* relativeTo);

// Integers
template<>
inline int Converter<int>::parse(const std::string &payload) {
    size_t pos = 0;
    int val = std::stoi(payload, &pos);
    if (pos < payload.size()) {
        throw std::invalid_argument("unparsable suffix: "
                                    + payload.substr(pos));
    }
    return val;
}
template<>
inline int Converter<int>::parse(const std::string &payload, const int& previous) {
    // rfind() returns the right-most occurrence of "+=",
    // but we start with (the leftmost) index 0, so checking
    // the return value rfind for 0 is equivalent to "startswith"
    if (payload.rfind("+=", 0) == 0) {
        return previous + parse(payload.substr(2));
    } else if (payload.rfind("-=", 0) == 0) {
        return previous - parse(payload.substr(2));
    } else {
        return parse(payload);
    }
}

template<>
unsigned long Converter<unsigned long>::parse(const std::string &source);

template<>
inline unsigned long Converter<unsigned long>::parse(const std::string &payload, const unsigned long& previous) {
    if (payload.rfind("+=", 0) == 0 || payload.rfind("-=", 0) == 0) {
        int delta = Converter<int>::parse(payload.substr(2));
        if (payload[0] == '-') {
            delta *= -1;
        }
        if (delta >= 0) {
            return previous + static_cast<unsigned long>(delta);
        } else {
            // negative delta:
            unsigned long toSubtract = static_cast<unsigned long>(-delta);
            if (toSubtract >= previous) {
                return 0;
            } else {
                return previous - toSubtract;
            }
        }
    } else {
        return parse(payload);
    }
}


// Booleans
template<>
inline std::string Converter<bool>::str(bool payload) {
    return { payload ? "true" : "false" };
}
template<>
inline bool Converter<bool>::parse(const std::string &payload) {
    std::set<std::string> t = {"true", "on", "1"};
    std::set<std::string> f = {"false", "off", "0"};
    if (f.find(payload) != f.end()) {
        return false;
    }
    if (t.find(payload) != t.end()) {
        return true;
    }
    throw std::invalid_argument("only on/off/true/false are valid booleans");
}
template<>
inline bool Converter<bool>::parse(const std::string &payload, const bool& previous) {
    if (payload == "toggle") {
        return !previous;
    }
    try {
        return parse(payload);
    } catch (std::invalid_argument&) {
        throw std::invalid_argument("only on/off/true/false/toggle are valid booleans");
    }
}

template<> void Converter<bool>::complete(Completion& complete, bool const* relativeTo);

// Strings
template<>
inline std::string Converter<std::string>::str(std::string payload) { return payload; }
template<>
inline std::string Converter<std::string>::parse(const std::string &payload) {
    return payload;
}

// Directions (used in frames, floating)
enum class Direction { Right, Left, Up, Down };
template<>
inline Direction Converter<Direction>::parse(const std::string &payload) {
    std::map<char, Direction> mapping = {
        {'u', Direction::Up},   {'r', Direction::Right},
        {'d', Direction::Down}, {'l', Direction::Left},
    };
    auto it = mapping.find(payload.at(0));
    if (it == mapping.end()) {
        throw std::invalid_argument("Invalid direction \"" + payload + "\"");
    }
    return it->second;
}

template<> void Converter<Direction>::complete(Completion& complete, Direction const* relativeTo);

enum class DirectionLevel { Frame = 0, Visible = 1, Tabs = 2, All = 3 };

template<>
DirectionLevel Converter<DirectionLevel>::parse(const std::string &payload);

template<>
std::string Converter<DirectionLevel>::str(const DirectionLevel d);

// Note: include x11-types.h for colors
