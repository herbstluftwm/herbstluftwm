#include "types.h"

Input &Input::operator>>(std::string &val)
{
    ArgList::operator>>(val);
    return *this;
}

Input Input::fromHere()
{
    std::string cmd;
    if (!(*this >> cmd))
        return {{}, {}};

    return Input(cmd, toVector());
}

void Input::replace(const std::string &from, const std::string &to)
{
    for (auto &v : *c_)
        if (v == from)
            v = to;

    if (*command_ == from)
        *command_ = to;
}
