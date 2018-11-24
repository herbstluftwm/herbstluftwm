#ifndef HERBSTLUFT_ARGLIST_H
#define HERBSTLUFT_ARGLIST_H

#include <vector>
#include <string>
#include <sstream>
#include <memory>

struct ArgList {
    using Container = std::vector<std::string>;

    // a simple split(), as C++ doesn't have it
    static Container split(const std::string &s, char delim = '.');

    // a simple join(), as C++ doesn't have it
    static std::string join(Container::const_iterator first,
                            Container::const_iterator last,
                            char delim = '.');

    ArgList(const std::initializer_list<std::string> &l);
    ArgList(const Container &c);
    // constructor that splits the given string
    ArgList(const std::string &s, char delim = '.');
    // operator to obtain shifted version of list (shallow copy)
    ArgList operator+(Container::difference_type shift_amount);
    std::string operator[](size_t idx);

    Container::const_iterator begin() const { return begin_; }
    Container::const_iterator end() const { return c_->cend(); }
    const std::string& front() { return *begin_; }
    const std::string& back() { return c_->back(); }
    bool empty() const { return begin_ == c_->end(); }
    Container::size_type size() const { return std::distance(begin_, c_->cend()); }

    std::string join(char delim = '.');

    void reset() { begin_ = c_->cbegin(); }
    void shift(Container::difference_type amount = 1) {
        begin_ += std::min(amount, std::distance(begin_, c_->cend()));
    }
    Container toVector() const {
        return Container(begin_, c_->cend());
    }
    /** try to read as many values as in target. If this fails
     * the original shift is restored
     */
    bool read(std::initializer_list<std::string*> targets);
    /** construct a new ArgList with every occurence of 'from' replaced by 'to'
     */
    ArgList replace(const std::string& from, const std::string& to);
    // the first element without any shifts.
    std::string command() const {
        if (c_->begin() != c_->end()) {
            return *c_->begin();
        } else {
            return {};
        }
    }

protected:
    Container::const_iterator begin_;
    /* shared pointer to make object copy-able:
     * 1. payload is shared (no redundant copies)
     * 2. begin_ stays valid
     */
    std::shared_ptr<Container> c_;
};

#endif
