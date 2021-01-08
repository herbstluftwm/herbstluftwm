#ifndef HLWM_ARGPARSE_H
#define HLWM_ARGPARSE_H

#include <functional>

#include "commandio.h"
#include "ipc-protocol.h"
#include "types.h"

/**
 * @brief The InputConvert class is a convenience wrapper around Input::operator>> and
 * Converter<X>::parse(). If an argument cannot be parsed, a meaningful error message
 * is printed to the output stream.
 */
class ArgParse
{
public:
    ArgParse() {
    }

    bool parsingFails(Input& input, Output& output);
    bool parsingAllFails(Input& input, Output& output);

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
     * @brief A flag is a command line flag (prefixed with one or two dashes)
     * and without a parameter. If a flag is given, the callback
     * function is called.
     */
    class Flag {
    public:
        Flag(std::string name, std::function<void()> callback)
            : name_(name)
            , callback_(callback)
        {}
        //! directly activate a boolean variable
        Flag(std::string name, bool* target)
            : name_(name)
        {
            callback_ = [target] () {
                if (target) {
                    *target = true;
                }
            };
        }

        std::string name_;
        std::function<void()> callback_;
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

    ArgParse& flags(std::initializer_list<Flag> flagTable);

    int exitCode() const { return errorCode_; }

private:
    bool unparsedTokens(Input& input, Output& output);
    bool tryParseFlag(std::string inputToken);
    std::vector<Argument> arguments_;
    std::map<std::string, Flag> flags_;
    int errorCode_ = 0;
};

#endif // HLWM_ARGPARSE_H
