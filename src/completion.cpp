#include "completion.h"
#include <string>
#include <vector>


/** Construct a completion context
 *
 * args         The arguments (without the command name)
 * index        The index of the argument to complete
 * shellOutput  Wether the output is shell encoded, see class description
 */
Completion::Completion(ArgList args, size_t index, bool shellOutput, Output output)
    : args_(args)
    , index_(index)
    , output_(output)
    , shellOutput_(shellOutput)
{
}

Completion::Completion(const Completion& other)
    : args_(other.args_)
    , index_(other.index_)
    , output_(other.output_)
    , shellOutput_(other.shellOutput_)
{
}

void Completion::operator=(const Completion& other) {
}

void Completion::full(const std::string& word) {
    output_ << word << " \n";
}

void Completion::full(const std::vector<std::string>& wordList) {
    for (auto& w : wordList) {
        full(w);
    }
}

void Completion::none() {
}

