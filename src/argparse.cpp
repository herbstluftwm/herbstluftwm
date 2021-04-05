#include "argparse.h"

#include <iostream>

#include "completion.h"

using std::function;
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
        while (optionalArgumentsRemaining) {
            try {
                if (!tryParseFlag(valueString)) {
                    // if it is not a flag, then break, i.e
                    // continue with positional args
                    break;
                }
            }  catch (std::exception& e) {
                // if there is a parse error however, then
                // stop entire parsing process
                output << input.command() << ": Cannot parse flag \""
                       << valueString << "\": " << e.what() << "\n";
                errorCode_ = HERBST_INVALID_ARGUMENT;
                return true;
            }
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
        try {
            if (tryParseFlag(input.front())) {
                input.shift();
            } else {
                break;
            }
        }  catch (std::exception& e) {
            output << input.command() << ": Cannot parse flag \""
                   << input.front() << "\": " << e.what() << "\n";
            errorCode_ = HERBST_INVALID_ARGUMENT;
            return true;
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

void ArgParse::command(CallOrComplete invocation, function<int(Output)> command)
{
    if (invocation.complete_) {
        completion(*(invocation.complete_));
    }
    if (invocation.inputOutput_) {
        int status = 0;
        if (parsingAllFails(invocation.inputOutput_->first,
                            invocation.inputOutput_->second)) {
            status = exitCode();
        } else {
            // if parsing did not fail
            status = command(invocation.inputOutput_->second);
        }
        if (invocation.exitCode_) {
            *(invocation.exitCode_) = status;
        }
    }
}

void ArgParse::completion(Completion& complete)
{
    size_t completionIndex = complete.index();
    std::set<Flag*> flagsPassedSoFar;
    for (size_t i = 0; i < completionIndex; i++) {
        Flag* flag = findFlag(complete[i]);
        if (flag) {
            flagsPassedSoFar.insert(flag);
        }
    }
    if (completionIndex - flagsPassedSoFar.size() > arguments_.size()) {
        // if there were too many arguments passed
        // already, then don't allow further arguments
        complete.none();
        return;
    }
    bool argsStillPossible = false;
    // complete the unused flags:
    for (auto& it : flags_) {
        if (flagsPassedSoFar.find(&(it.second)) == flagsPassedSoFar.end()) {
            // complete names of flags that were not mentioned so far
            it.second.complete(complete);
            argsStillPossible = true;
        }
    }
    // the index of the current argument when neglecting
    // the flags
    size_t positionalIndex = completionIndex - flagsPassedSoFar.size();
    // we iterate through all arguments_ to determine which of them
    // might be possible at positionalIndex
    size_t minPossibleIdx = 0;
    size_t maxPossibleIdx = 0;
    for (const auto& arg : arguments_) {
        if (minPossibleIdx <= positionalIndex
            && positionalIndex <= maxPossibleIdx)
        {
            arg.complete_(complete);
            for (const auto& suggestion : arg.completionSuggestions_) {
                complete.full(suggestion);
            }
            argsStillPossible = true;
        }
        maxPossibleIdx++;
        if (!arg.optional_) {
            minPossibleIdx++;
        }
    }
    if (!argsStillPossible) {
        complete.none();
    }
}

/**
 * @brief try parse a flag, possibly throwing an exception
 * on a parse error.
 * @param argument token from a Input object
 * @return whether the token was a flag
 */
bool ArgParse::tryParseFlag(const string& inputToken)
{
    Flag* flag = findFlag(inputToken);
    if (!flag) {
        // stop parsing flags on the first non-flag
        return false;
    }
    string parameter = "";
    if (inputToken.size() != flag->name_.size()) {
        parameter = inputToken.substr(flag->name_.size());
    }
    flag->callback_(parameter);
    return true;
}

/**
 * @brief Find a flag that is appropriate for a given inputToken
 * @param the token
 * @return A flag or a nullptr
 */
ArgParse::Flag* ArgParse::findFlag(const std::string& inputToken)
{
    string needle = inputToken;
    size_t separatorPos = inputToken.find("=");
    if (separatorPos != string::npos) {
        needle = inputToken.substr(0, separatorPos + 1);
    }
    auto flag = flags_.find(needle);
    if (flag == flags_.end()) {
        return nullptr;
    } else {
        return &(flag->second);
    }
}

void ArgParse::Flag::complete(Completion& completion)
{
    if (!name_.empty() && *name_.rbegin() == '=') {
        // complete partially with argument
        completion.partial(name_);
        // use completion
        completion.withPrefix(name_, parameterTypeCompletion_);
    } else {
        // ordinary flag that is either present or absent:
        completion.full(name_);
    }
}
