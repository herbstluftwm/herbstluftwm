#ifndef INPUTCONVERT_H
#define INPUTCONVERT_H

#include <functional>

#include "ipc-protocol.h"
#include "types.h"

/**
 * @brief The InputConvert class is a convenience wrapper around Input::operator>> and
 * Converter<X>::parse(). If an argument can not be parsed, a meaningful error message
 * is printed to the output stream.
 */
class ArgParse
{
public:
    ArgParse() {
    }

    bool parseOrExit(Input& input, Output& output);

    class Argument {
    public:
        /** try to parse the argument from the given string or
         * throw an exception
         */
        std::function<void(std::string)> tryParse_;
        //! whether the present argument is only optional
        bool optional_;
    };

    /**
     * Defines a mandatory argument of type X
     */
    template<typename X>
    ArgParse& mandatory(X& value) {
        Argument arg {
            [&value] (std::string source) {
                value = Converter<X>::parse(source);
            },
            false};
        arguments_.push_back(arg);
        return *this;
    }

    /**
     * Defines an optional argument of type X.
     * If there are more arguments in the input than there are
     * mandatory arguments, then the optional arguments are filled with
     * input (earlier optional arguments are preferred).
     * The target of the whetherArgumentSupplied pointer is set to 'true'
     * if the present optional argument was supplied in the input.
     */
    template<typename X>
    ArgParse& optional(X& value, bool* whetherArgumentSupplied = nullptr) {
        if (whetherArgumentSupplied) {
            *whetherArgumentSupplied = false;
        }
        Argument arg {
            [&value, whetherArgumentSupplied] (std::string source) {
                value = Converter<X>::parse(source);
                if (whetherArgumentSupplied) {
                    *whetherArgumentSupplied = true;
                }
            },
            true};
        arguments_.push_back(arg);
        return *this;
    }

    int exitCode() const { return errorCode_; }

private:
    std::vector<Argument> arguments_;
    int errorCode_ = 0;
};

#endif // INPUTCONVERT_H
