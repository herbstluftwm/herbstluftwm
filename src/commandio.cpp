#include "commandio.h"

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
