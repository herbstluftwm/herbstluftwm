#include "arglist.h"

using namespace std;

ArgList::ArgList(const std::initializer_list<std::string> &l)
    : c_(std::make_shared<Container>(l)) { reset(); }

ArgList::ArgList(const ArgList::Container &c) : c_(std::make_shared<Container>(c)) { reset(); }

ArgList::ArgList(const ArgList &al) : c_(al.c_) { reset(); }

ArgList::ArgList(const std::string &s, char delim) {
    c_ = std::make_shared<Container>(split(s, delim));
    reset();
}

ArgList::Container ArgList::split(const std::string &s, char delim) {
    Container ret;
    std::stringstream tmp(s);
    std::string item;
    // read "lines" seperated by delim
    while (std::getline(tmp, item, delim))
        ret.push_back(item);
    // with this, there is no distinction whether there was a delim
    // in the end or not; so fix this manually:
    if (!s.empty() && s.back() == delim) {
        ret.push_back("");
    }
    return ret;
}

std::string ArgList::join(ArgList::Container::const_iterator first,
                          ArgList::Container::const_iterator last,
                          char delim) {
    if (first == last)
        return {};
    std::stringstream tmp;
    tmp << *first;
    for (auto it = first + 1; it != last; ++it)
        tmp << delim << *it;
    return tmp.str();
}
std::string ArgList::join(char delim) {
    return join(begin_, c_->cend(), delim);
}

ArgList &ArgList::operator>>(string &val) {
    if (!empty()) {
        val = front();
        shift();
    } else {
        shiftedTooFar_ = true;
    }
    return *this;
}
