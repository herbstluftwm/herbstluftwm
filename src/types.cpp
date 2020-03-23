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
    if (!(*this >> cmd))
        return {{}, {}};

    return Input(cmd, toVector());
}

void Input::replace(const string &from, const string &to)
{
    for (auto &v : *container_)
        if (v == from)
            v = to;

    if (*command_ == from)
        *command_ = to;
}

template<> void Converter<bool>::complete(Completion& complete, bool const* relativeTo)
{
    complete.full({ "on", "off", "true", "false" });
    if (relativeTo) complete.full("toggle");
}

void completeFull(Completion &complete, std::string s)
{
    complete.full(s);
}
