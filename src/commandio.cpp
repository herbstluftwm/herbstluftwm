#include "commandio.h"

#include <iostream>

#include "completion.h"
#include "globals.h"

using std::string;
using std::endl;

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

OutputChannels OutputChannels::stdio() {
    return OutputChannels(WINDOW_MANAGER_NAME, std::cout, std::cerr);
}

std::ostream& OutputChannels::perror()
{
    return error_ << commandName_ << ": ";
}
