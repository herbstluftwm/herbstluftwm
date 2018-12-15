#include "completion.h"

Completion::Completion(ArgList args, size_t index)
    : args_(args)
    , index_(index)
{
}

Completion::Completion(const Completion& other)
    : args_(other.args_)
    , index_(other.index_)
{
}

void Completion::operator=(const Completion& other)
{
}
