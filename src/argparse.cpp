#include "argparse.h"

#include <iostream>

using std::string;

/**
 * @brief try to parse the arguments
 * @param input
 * @param output
 * @return return whether there has been an error (true = error, false = no error)
 */
bool ArgParse::parseOrExit(Input& input, Output& output)
{
    size_t mandatoryArguments = 0;
    size_t optionalArguments = 0;
    for (const auto& arg : arguments_) {
        if (arg.optional_) {
            optionalArguments++;
        } else {
            mandatoryArguments++;
        }
    }
    if (input.size() < mandatoryArguments) {
        output << input.command() << ": Expected ";
        if (optionalArguments) {
            output << "between " << mandatoryArguments
                   << " and " << (optionalArguments + mandatoryArguments)
                   << " arguments";
        } else if (mandatoryArguments == 1) {
            output  << "one argument";
        } else {
            output  << mandatoryArguments << " arguments";
        }
        output << ", but got only " << input.size() << " arguments.";
        errorCode_ = HERBST_NEED_MORE_ARGS;
        return true;
    }
    // the number of optional arguments that are provided by 'input':
    size_t optionalArgumentsRemaining = input.size() - mandatoryArguments;
    for (const auto& arg : arguments_) {
        if (arg.optional_) {
            if (optionalArgumentsRemaining) {
                optionalArgumentsRemaining--;
            } else {
                // skip this argument if it's optional and there
                // are not more optional arguments in the input
                continue;
            }
        }
        string valueString;
        if (!(input >> valueString)) {
            // this should not happen
            errorCode_ = HERBST_NEED_MORE_ARGS;
            return true;
        }
        try {
            arg.tryParse_(valueString);
        }  catch (std::exception& e) {
            output << input.command() << ": Cannot parse argument \""
                   << valueString << "\": " << e.what() << "\n";
            errorCode_ = HERBST_INVALID_ARGUMENT;
            return true;
        }
    }
    // if all arguments were parsed, then we report that there were
    // no errors. It's ok if there are remaining elements in 'input' that
    // have not been parsed.
    return false;
}
