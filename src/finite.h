#pragma once

#include <type_traits>

#include "completion.h"
#include "types.h"

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

    using ValueListPlain = std::vector<std::pair<T, std::string>>;
    class ValueList : public ValueListPlain {
    public:
        ValueList(ValueListPlain plainValueList)
            : ValueListPlain(plainValueList)
        {}

        ValueList(bool allowIndicesAsNames, ValueListPlain plainValueList)
            : ValueListPlain(plainValueList)
            , allowIndicesAsNames_(allowIndicesAsNames)
        {}

        /**
         * @brief Specifies whether the parsing accepts integer
         * indices in the value list. Usually, this should only be
         * activated (return true) for the sake of compatibility.
         * Thus, there is no completion for indices.
         */
        bool allowIndicesAsNames_ = false;
    };
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
        if (values.allowIndicesAsNames_) {
            bool isInteger =
                    !payload.empty() &&
                    payload.find_first_not_of("0123456789") == std::string::npos;
            if (isInteger) {
                size_t index = static_cast<size_t>(std::stoi(payload));
                if (index >= values.size()) {
                    std::string msg
                            = "index \"" + payload + "\" out of range"
                            + " (must be less than "
                            + std::to_string(values.size()) + ")";
                    throw std::invalid_argument(msg);
                }
                return values[index].first;
            }
        }
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

template <typename T>
struct is_finite : std::false_type {};

/**
 * A Converter implementation for types XX which are 'finite'. To make this applicable
 * to a type XX, one needs to:
 *
 *   1. define the static variable:
 *
 *      Finite<XX>::values;
 *
 *   2. make the type fulfill the is_finite predicate:
 *
 *     template <>
 *     struct is_finite<XX> : std::true_type {};
 */
template <typename T>
class Converter<T, typename std::enable_if< is_finite<T>::value >::type> {
public:
    static T parse(const std::string& source) {
        return Finite<T>::parse(source);
    }
    static T parse(const std::string& source, const T&) {
        return parse(source);
    };
    static std::string str(T payload) {
        return Finite<T>::str(payload);
    }
    static void complete(Completion& complete, T const* relativeTo) {
        Finite<T>::complete(complete, relativeTo);
    }
    static void complete(Completion& complete) {
        Finite<T>::complete(complete, nullptr);
    }
};

