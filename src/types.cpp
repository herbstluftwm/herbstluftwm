#include "types.h"


Input::Input(const ArgList& argv)
    : ArgList(argv)
{
}

Input& Input::operator>>(std::string& val) {
    // if we are on the beginning, skip the command name;
    if (c_->begin() == begin_) {
        shift();
    }
    ArgList::operator>>(val);
    return *this;
}

std::string Input::command() const {
    if (c_->cbegin() != c_->cend()) {
        return *c_->cbegin();
    }
    // access the first element without any shifts.
    if (c_->begin() != c_->end()) {
        return *c_->begin();
    } else {
        return {};
    }
}
