#include "types.h"
#include "completion.h"

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
