#pragma once

#include "completion.h"

/** The Finite<T> class defines the Converter methods
 * for enum class types, that is, for types with only finitely many values.
 *
 * In order to use it, one needs to define a table
 * static Finite<T>::ValueList values = { ... };
 * and then one can call the static functions below when
 * implementing the Converter for T.
 */
template<typename T>
class Finite {
public:
    using ValueList = std::vector<std::pair<T, std::string>>;
    static ValueList values;
    static std::string str(T cp) {
        for (const auto& p : values) {
            if (cp == p.first) {
                return p.second;
            }
        }
        return "Internal error: Unknown value (this must not happen!)";
    }
    static T parse(const std::string& payload) {
        for (const auto& p : values) {
            if (payload == p.second) {
                return p.first;
            }
        }
        std::string message = "Expecting one of: ";
        // show at most that many possible values
        size_t elements = 5;
        bool first = true;
        for (const auto& p : values) {
            if (!first) {
                message += ", ";
            }
            first = false;
            if (elements > 0) {
                message += p.second;
                elements--;
            } else {
                message += "...";
                break;
            }
        }
        throw std::invalid_argument(message);
    }
    static void complete(Completion& complete, T const*) {
        for (const auto& p : values) {
            complete.full(p.second);
        }
    }
};
