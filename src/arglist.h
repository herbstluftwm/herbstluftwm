#ifndef HERBSTLUFT_ARGLIST_H
#define HERBSTLUFT_ARGLIST_H

#include <vector>
#include <string>
#include <sstream>
#include <memory>

class ArgList {

public:
    using Container = std::vector<std::string>;

    //! a simple split(), as C++ doesn't have it
    static Container split(const std::string &s, char delim = '.');

    //! a simple join(), as C++ doesn't have it
    static std::string join(Container::const_iterator first,
                            Container::const_iterator last,
                            char delim = '.');

    ArgList(const std::initializer_list<std::string> &l);
    ArgList(const ArgList &al);
    ArgList(const Container &c);
    // constructor that splits the given string
    ArgList(const std::string &s, char delim = '.');

    Container::const_iterator begin() const { return begin_; }
    Container::const_iterator end() const { return c_->cend(); }
    const std::string& front() { return *begin_; }
    const std::string& back() { return c_->back(); }
    bool empty() const { return begin_ == c_->end(); }
    Container::size_type size() const { return std::distance(begin_, c_->cend()); }

    std::string join(char delim = '.');

    //! reset internal pointer to begin of arguments
    void reset() {
        begin_ = c_->cbegin();
        shiftedTooFar_ = false;
    }
    //! shift the internal pointer by amount
    void shift(Container::difference_type amount = 1) {
        begin_ += std::min(amount, std::distance(begin_, c_->cend()));
    }
    Container toVector() const {
        return Container(begin_, c_->cend());
    }
    //! try read a value if possible
    ArgList& operator>>(std::string& val) {
        if (!empty()) {
            val = front();
            shift();
        } else {
            shiftedTooFar_ = true;
        }
        return *this;
    }
    //! tell whether all previous operator>>() succeeded
    operator bool() const {
        return !shiftedTooFar_;
    }
    //! construct a new ArgList with every occurence of 'from' replaced by 'to'
    ArgList replaced(const std::string& from, const std::string& to) const;

protected:
    //! shift state pointing into c_
    Container::const_iterator begin_;
    //! indicator that we attempted to shift too far (shift is at end())
    bool shiftedTooFar_ = false;
    /*! Argument vector
     * @note This is a shared pointer to make object copy-able:
     * 1. payload is shared (no redundant copies)
     * 2. begin_ stays valid
     */
    std::shared_ptr<Container> c_;
};

#endif
