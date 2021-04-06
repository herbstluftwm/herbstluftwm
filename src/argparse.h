#ifndef HLWM_ARGPARSE_H
#define HLWM_ARGPARSE_H

#include <functional>
#include <string>
#include <vector>

#include "commandio.h"
#include "converter.h"
#include "ipc-protocol.h"

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

    using WordList = std::vector<std::string>;

    class Argument {
    public:
        /** try to parse the argument from the given string or
         * throw an exception
         */
        std::function<void(std::string)> tryParse_;
        std::function<void(Completion&)> complete_;
        //! in addition to the completion function suggested completions
        WordList completionSuggestions_;
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
            , callback_([callback](const std::string&) { callback(); })
        {}
        //! directly activate a boolean variable
        Flag(std::string name, bool* target)
            : name_(name)
        {
            callback_ = [target] (const std::string&) {
                if (target) {
                    *target = true;
                }
            };
        }
        //! a flag with a parameter
        template<typename X>
        Flag(const std::string& name, X& target)
            : name_(name)
        {
            callback_ = [&target] (const std::string& source) {
                target = Converter<X>::parse(source);
            };
            parameterTypeCompletion_ = [] (Completion& completion) {
                Converter<X>::complete(completion, nullptr);
            };
        }

        void complete(Completion& completion);

        //! if it is sufficient that name_ is only
        //! a prefix
        std::string name_;
        //! the callback is invoked with a possible parameter
        std::function<void(const std::string&)> callback_;
        std::function<void(Completion&)> parameterTypeCompletion_;
    };

    /**
     * Defines a mandatory argument of type X
     */
    template<typename X>
    ArgParse& mandatory(X& value, WordList completionSuggestions = {}) {
        Argument arg {
            [&value] (std::string source) {
                value = Converter<X>::parse(source);
            },
            [] (Completion& complete) {
                Converter<X>::complete(complete, nullptr);
            },
            completionSuggestions,
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
    ArgParse& optional(X& value, WordList completionSuggestions, bool* whetherArgumentSupplied) {
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
            [] (Completion& complete) {
                Converter<X>::complete(complete, nullptr);
            },
            completionSuggestions,
            true};
        arguments_.push_back(arg);
        return *this;
    }

    template<typename X>
    ArgParse& optional(X& value, WordList completionSuggestions = {}) {
        return optional(value, completionSuggestions, nullptr);
    }

    template<typename X>
    ArgParse& optional(X& value, bool* whetherArgumentSupplied) {
        return optional(value, {}, whetherArgumentSupplied);
    }

    ArgParse& flags(std::initializer_list<Flag> flagTable);

    int exitCode() const { return errorCode_; }

    void command(CallOrComplete invocation, std::function<int(Output)> command);

private:
    void completion(Completion& complete);
    bool unparsedTokens(Input& input, Output& output);
    bool tryParseFlag(const std::string& inputToken);
    Flag* findFlag(const std::string& inputToken);
    std::vector<Argument> arguments_;
    std::map<std::string, Flag> flags_;
    int errorCode_ = 0;
};

#endif // HLWM_ARGPARSE_H
