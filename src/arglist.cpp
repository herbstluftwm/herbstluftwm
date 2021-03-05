#include "arglist.h"

#include <sstream>

#include "utils.h"

using std::make_shared;
using std::string;
using std::stringstream;

ArgList::ArgList(const ArgList::Container &c)
    : container_(make_shared<Container>(c))
{ reset(); }

ArgList::ArgList(const ArgList &al) : container_(al.container_) { reset(); }

ArgList::ArgList(const string &s, char delim) {
    container_ = make_shared<Container>(split(s, delim));
    reset();
}

ArgList::ArgList(Container::const_iterator from, Container::const_iterator to)
{
    container_ = make_shared<Container>(from, to);
    reset();
}

ArgList::Container ArgList::split(const string &s, char delim) {
    Container ret;
    stringstream tmp(s);
    string item;
    // read "lines" seperated by delim
    while (std::getline(tmp, item, delim)) {
        ret.push_back(item);
    }
    // with this, there is no distinction whether there was a delim
    // in the end or not; so fix this manually:
    if (!s.empty() && s.back() == delim) {
        ret.push_back("");
    }
    return ret;
}

string ArgList::join(char delim) {
    return join_strings(*this, {delim});
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
