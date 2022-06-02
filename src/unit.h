#ifndef UNIT_H
#define UNIT_H

#include "completion.h"
#include "converter.h"

#include <type_traits>

/**
 * @brief The Unit class represents a type that
 * has only one inhabitant
 */
template<const char name[]>
class Unit {
public:
    bool operator==(const Unit& other) { return true; }
};

template <const char name[]>
class Converter<Unit<name>, typename std::enable_if< std::true_type::value >::type> {
public:
    static inline std::string str(Unit<name> payload) { return name; }

    static inline Unit<name> parse(const std::string &payload) {
        if (payload == name) {
            return {};
        }
        throw std::invalid_argument(std::string("Expected \"") + name + "\"");
    }

    static void complete(Completion& complete, Unit<name> const* relativeTo) {
        completeFull(complete, name);
    }
};



#endif // UNIT_H
