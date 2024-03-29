#include "completion.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "utils.h"

using std::function;
using std::string;

/** Construct a completion context
 *
 * args         The arguments (without the command name)
 * index        The index of the argument to complete
 * shellOutput  Wether the output is shell encoded, see class description
 */
Completion::Completion(ArgList args, size_t index, const string& prepend, bool shellOutput, Output output)
    : args_(args)
    , index_(index)
    , prepend_(prepend)
    , output_(output)
    , shellOutput_(shellOutput)
{
    if (index_ < args_.size()) {
        needle_ = args_.toVector()[index_];
    } else {
        needle_ = "";
    }
}

Completion::Completion(const Completion& other)
    : Completion(other.args_, other.index_, other.prepend_, other.shellOutput_, other.output_)
{
}

void Completion::operator=(const Completion& other) {
}

void Completion::full(const string& word) {
    if (prefixOf(needle_, prepend_ + word)) {
        entryCount_++;
        output_ << escape(prepend_ + word) << (shellOutput_ ? " \n" : "\n");
        // std::cout << "add " << word << endl;
    } else {
        // std::cout << needle_ << " not prefix of " << word << endl;
    }
}

void Completion::full(const std::initializer_list<string>& wordList) {
    for (auto& w : wordList) {
        full(w);
    }
}

void Completion::none() {
    noParameterExpected_ = true;
}

void Completion::parametersStillExpected()
{
    noParameterExpected_ = false;
}

//! Return the given string posix sh escaped if in shell output mode
string Completion::escape(const string& str) {
    return shellOutput_ ? posix_sh_escape(str) : str;
}

//! the requested position is beyond the number of expected parameters
bool Completion::noParameterExpected() const {
    return noParameterExpected_;
}

bool Completion::prefixOf(const string& shorter, const string& longer)
{
    auto res = std::mismatch(shorter.begin(), shorter.end(), longer.begin());
    return res.first == shorter.end();
}

void Completion::partial(const string& word) {
    if (prefixOf(needle_, prepend_ + word)) {
        // partial completions never end with a space, regardless of
        // shellOutput mode
        output_ << escape(prepend_ + word) << "\n";
        entryCount_++;
    }
}

const string& Completion::needle() const
{
    return needle_;
}

/** get a positional argument in the current completion situation
 *
 * if for a int 'index' the expression operator==(index) is true, then
 * operator[](index) is the same as needle();
 */
string Completion::operator[](size_t index) const {
    auto it = args_.begin() + index;
    if (it == args_.end()) {
        return "";
    } else {
        return *it;
    }
}

/** create a new Completion context with the first 'offset' args dropped.
 * the offset can be at most the index_. That is for every size_t offs
 * if operator>=(offs) is true, then shifted(offs) is save.
 */
Completion Completion::shifted(size_t offset) const {
    if (offset > index_) {
        throw std::logic_error("Can shift at most index_ many elements!");
    }
    offset = std::min(offset, args_.size());
    return Completion(
        ArgList(args_.begin() + offset, args_.end()),
        index_ - offset,
        prepend_,
        shellOutput_,
        output_);
}

//! Complete against all available commands, starting at the given offset
void Completion::completeCommands(size_t offset) {
    Completion thisShifted = shifted(offset);
    Commands::complete(thisShifted);
    mergeResultsFrom(thisShifted);
}

void Completion::mergeResultsFrom(Completion& source)
{
    entryCount_ += source.entryCount();
    if (source.ifInvalidArguments()) {
        invalidArguments();
    }
    if (source.noParameterExpected()) {
        none();
    }
}

void Completion::withPrefix(const string& prependPrefix, function<void (Completion&)> callback)
{
    Completion withPrefix(
        args_,
        index_,
        prepend_ + prependPrefix,
        shellOutput_,
        output_);
    callback(withPrefix);
    mergeResultsFrom(withPrefix);
}

size_t Completion::entryCount() const
{
    return entryCount_;
}

void Completion::invalidArguments() {
    invalidArgument_ = true;
}

bool Completion::ifInvalidArguments() const {
    return invalidArgument_;
}

