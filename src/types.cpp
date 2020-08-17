#include "types.h"

#include "completion.h"

using std::string;

Input &Input::operator>>(string &val)
{
    ArgList::operator>>(val);
    return *this;
}

Input Input::fromHere()
{
    string cmd;
    if (empty()) {
        return {{}, {}};
    }

    return Input(*(begin()), Container(begin() + 1, end()));
}

void Input::replace(const string &from, const string &to)
{
    for (auto &v : *container_) {
        if (v == from) {
            v = to;
        }

        if (*command_ == from) {
            *command_ = to;
        }
    }
}

template<> void Converter<bool>::complete(Completion& complete, bool const* relativeTo)
{
    complete.full({ "on", "off", "true", "false" });
    if (relativeTo != nullptr) {
        complete.full("toggle");
    }
}

void completeFull(Completion &complete, string s)
{
    complete.full(s);
}

template<>
void Converter<Direction>::complete(Completion& complete, const Direction* relativeTo)
{
    complete.full({"up", "down", "left", "right"});
}

template<>
unsigned long Converter<unsigned long>::parse(const string& source)
{
    long value = std::stol(source);
    if (value < 0) {
        throw std::invalid_argument("negative number is out of range");
    } else {
        return (unsigned long)(value);
    }
}
