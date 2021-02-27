#pragma once

#include <type_traits>

#include "types.h"

/**
 * the same definition as 'Converter', but without the 'static'
 */
template <typename T>
class RunTimeConverter {
public:
    virtual T parse(const std::string& source) = 0;
    virtual std::string str(T payload) = 0;
    virtual void complete(Completion& completion) = 0;
};

/**
 * A converter for pointer types T, e.g. Client*, Tag*, Monitor*.
 * A converter for such types has callbacks to the respective manager
 * which does the actual conversion and completion.
 */
template <typename T>
class Converter<T, typename std::enable_if< std::is_pointer<T>::value >::type> {
public:
    static RunTimeConverter<T>* converter;
    static T parse(const std::string& source) {
        if (converter) {
            return converter->parse(source);
        } else {
            throw std::invalid_argument("Converter not initialized!");
        }
    }
    static std::string str(T payload) {
        if (converter) {
            return converter->str(payload);
        } else {
            return "Error: Converter not initialized!";
        }
    }
    static void complete(Completion& completion) {
        if (converter) {
            converter->complete(completion);
        }
    }
    static void complete(Completion& completion, T const*) {
        complete(completion);
    }
};

/**
 * Inheriting from this in the manager will automatic register/unregister
 * the RunTimeConverter<T*> at the Converter<T*>.
 */
template <typename T>
class Manager : public RunTimeConverter<T*> {
public:
    Manager() {
        Converter<T*>::converter = this;
    }
    ~Manager() {
        Converter<T*>::converter = nullptr;
    }
};
