#include "arglist.h"

using namespace std;

ArgList::ArgList(const std::initializer_list<std::string> &l)
    : c_(std::make_shared<Container>(l)) { reset(); }

ArgList::ArgList(const ArgList::Container &c) : c_(std::make_shared<Container>(c)) { reset(); }

ArgList::ArgList(const std::string &s, char delim) {
    c_ = std::make_shared<Container>();
    split(*c_, s, delim);
    reset();
}

ArgList ArgList::operator+(Container::difference_type shift_amount) {
    ArgList ret(*this);
    ret.shift(shift_amount);
    return ret;
}

std::string ArgList::operator[](size_t idx) {
    return c_->operator[](idx);
}

void ArgList::split(Container &ret, const std::string &s, char delim) {
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
}

ArgList::Container ArgList::split(const std::string &s, char delim) {
    Container ret;
    split(ret, s, delim);
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


bool ArgList::read(std::initializer_list<std::string*> targets) {
    auto begin_backup = begin_;
    for (auto cur_target : targets) {
        if (empty()) {
            begin_ = begin_backup;
            return false;
        }
        *cur_target = front();
        shift();
    }
    return true;
}

ArgList ArgList::replace(const std::string& from, const std::string& to) {
    int i = 0;
    vector<std::string> new_list(size());
    for (auto v : *this) {
        if (v == from) new_list[i] = to;
        else new_list[i] = v;
        ++i;
    }
    return ArgList(new_list);
}

