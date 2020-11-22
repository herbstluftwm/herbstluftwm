#include "argparse.h"

#include <iostream>

using std::pair;
using std::string;

/**
 * @brief try to parse the positional arguments and as many flags
 * as possible. When the last positional argument has been read
 * then flags are still read until the first unknown token.
 * If unknown tokens at the end of input should be regarded as
 * an error, use parsingAllFails()
 * @param input
 * @param output
 * @return return whether there has been an error (true = error, false = no error)
 */
bool ArgParse::parsingFails(Input& input, Output& output)
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
    optionalArguments += flags_.size();
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
        if (arg.optional_ && optionalArgumentsRemaining == 0) {
            continue;
        }
        string valueString;
        input >> valueString;
        // try to parse this token as a flag
        if (optionalArgumentsRemaining && tryParseFlag(valueString)) {
            optionalArgumentsRemaining--;
            // if this token was the last optional argument
            if (optionalArgumentsRemaining == 0 && arg.optional_) {
                // then skip this 'arg'
                continue;
            }
            // otherwise, there is room for another optional
            // argument or 'arg' is mandatory. So get another
            // token from the input, and parse it to the
            // current 'arg'
            input >> valueString;
        }
        // in any case, we have this here:
        // assert (!arg.optional_ || optionalArgumentsRemaining > 0);
        if (arg.optional_) {
            optionalArgumentsRemaining--;
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
    // try to parse more flags after the positional arguments
    while (!input.empty()) {
        // only consume tokens if they are recognized flags
        if (tryParseFlag(input.front())) {
            input.shift();
        } else {
            break;
        }
    }

    // if all arguments were parsed, then we report that there were
    // no errors. It's ok if there are remaining elements in 'input' that
    // have not been parsed.
    return false;
}

/**
 * @brief run parsingFails() and assert that there are no unknown
 * tokens left in the input.
 * @param input
 * @param output
 * @return whether there is an unparsable flag or unexpected argument at the end
 */
bool ArgParse::parsingAllFails(Input& input, Output& output)
{
    return parsingFails(input, output) || unparsedTokens(input, output);
}

bool ArgParse::unparsedTokens(Input& input, Output& output)
{
    string extraToken;
    if (input >> extraToken) {
        output << input.command()
           << ": Unknown argument or flag \""
           << extraToken << "\" given.\n";
        errorCode_ = HERBST_INVALID_ARGUMENT;
        return true;
    }
    return false;
}

/**
 * @brief Accept boolean flags (e.g. --all --horizontal ...) at
 * any position between the (mandatory or optional) positional arguments
 * @param a list of flags
 * @return
 */
ArgParse& ArgParse::flags(std::initializer_list<Flag> flagTable)
{
    for (auto& it : flagTable) {
        // the usual array-style assignment does not work
        // because Flag has no parameterless constructor.
        // hence, we explicitly call 'insert':
        flags_.insert(pair<string, Flag>(it.name_, it));
    }
    return *this;
}

/**
 * @brief try parse a flag
 * @param argument token from a Input object
 * @return whether the token was a flag
 */
bool ArgParse::tryParseFlag(string inputToken)
{
    auto flag = flags_.find(inputToken);
    if (flag == flags_.end()) {
        // stop parsing flags on the first non-flag
        return false;
    }
    flag->second.callback_();
    return true;
}
