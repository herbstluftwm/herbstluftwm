#ifndef INPUTCONVERT_H
#define INPUTCONVERT_H

#include <iostream> // for the instance of operator<<(ostream, string)

#include "ipc-protocol.h"
#include "types.h"

/**
 * @brief The InputConvert class is a convenience wrapper around Input::operator>> and
 * Converter<X>::parse(). If an argument can not be parsed, a meaningful error message
 * is printed to the output stream.
 */
class InputConvert
{
public:
    InputConvert(Input& input, Output& output);

    class Optional {
    public:
    };

    /**
     * parse the next argument into the given value. If there is a parser error
     * then this sets the exit code and prints an error message to 'output'.
     * If there is no argument left in the input, then this is not treated as
     * an error if Optional() has been passed to the stream before.
     */
    template<typename X>
    InputConvert& operator>>(X& value) {
        if (errorCode_ != 0) {
            // if some error occured, don't try to parse further
            // values
            return *this;
        }
        std::string valueString;
        if (!(input_ >> valueString)) {
            // if there is no more command line argument,
            // we treat this as an error.
            // However, if the Optional-mark was passed
            // earlier, we don't count it as an error
            if (!onlyOptionalArgumentsRemain_) {
                errorCode_ = HERBST_NEED_MORE_ARGS;
            }
            return *this;
        }
        try {
            value = Converter<X>::parse(valueString);
        }  catch (std::exception& e) {
            output_ << input_.command() << ": Cannot parse argument \""
                    << valueString << "\": " << e.what() << "\n";
            errorCode_ = HERBST_INVALID_ARGUMENT;
            return *this;
        }
        return *this;
    }

    InputConvert& operator>>(Optional value) {
        onlyOptionalArgumentsRemain_ = true;
        return *this;
    }


    operator bool() const {
        return errorCode_ == 0;
    }
    operator int() const {
        return errorCode_;
    }
private:
    bool onlyOptionalArgumentsRemain_ = false;
    int errorCode_ = 0;
    Input& input_;
    Output& output_;
};

#endif // INPUTCONVERT_H
