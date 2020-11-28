#ifndef HERBSTLUFT_TYPES_H
#define HERBSTLUFT_TYPES_H

#include <map>
#include <set>

#include "arglist.h"

class Completion;

/* A path in the object tree */
using Path = ArgList;

/* Types for I/O with the user */
using Output = std::ostream&;

/** The Input for a command consists of a command name
 * (known as argv[0] in a C main()) and of arguments (argv[1] up to
 * argv[argc] in a C main()).
 *
 * This class is in transition and not yet in its final state!
 * Currently, this class behaves as the ArgList. But later, ArgList will become
 * protected and so one can only use >> to access the arguments
 *
 * This means that currently, the base class holds the entire argv. Later, the
 * base class will only hold the arguments and not the command name and
 * furthermore the ArgList parent class will become private.
 */
class Input : public ArgList {
public:
    Input(const std::string command, const Container &c = {})
        : ArgList(c), command_(std::make_shared<std::string>(command)) {}

    Input(const std::string command, Container::const_iterator from, Container::const_iterator to)
        : ArgList(from, to), command_(std::make_shared<std::string>(command)) {}

    //! create a new Input but drop already parsed arguments
    Input(const Input& other)
        : ArgList(other.toVector()), command_(other.command_) {}

    const std::string& command() const { return *command_; }

    Input &operator>>(std::string &val) override;

    //! construct a new Input where the first (current) arg is the command
    Input fromHere();

    //! Replace every occurence of 'from' by 'to'
    //! @note this includes the command itself
    void replace(const std::string &from, const std::string &to);

protected:
    //! Command name
    //! A shared pointer to avoid copies when passing Input around
    //! @note The C-style compatibility layer DEPENDS on the shared_ptr!
    std::shared_ptr<std::string> command_;
};

// only a trick such that we don't need to include completion.h here
void completeFull(Completion& complete, std::string string);

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
    return std::stoi(payload);
}
template<>
unsigned long Converter<unsigned long>::parse(const std::string &source);

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

// Note: include x11-types.h for colors

#endif
