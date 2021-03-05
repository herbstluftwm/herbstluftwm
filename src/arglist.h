#ifndef HERBSTLUFT_ARGLIST_H
#define HERBSTLUFT_ARGLIST_H

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

class ArgList {

public:
    using Container = std::vector<std::string>;

    //! a simple split(), as C++ doesn't have it
    static Container split(const std::string &s, char delim = '.');

    ArgList(Container::const_iterator from, Container::const_iterator to);
    ArgList(const ArgList &al);
    ArgList(const Container &c);
    // constructor that splits the given string
    ArgList(const std::string &s, char delim = '.');
    virtual ~ArgList() {}

    Container::const_iterator begin() const { return begin_; }
    Container::const_iterator end() const { return container_->cend(); }
    const std::string& front() { return *begin_; }
    const std::string& back() { return container_->back(); }
    bool empty() const { return begin_ == container_->end(); }
    Container::size_type size() const {
        return std::distance(begin_, container_->cend());
    }

    std::string join(char delim = '.');

    //! reset internal pointer to begin of arguments
    void reset() {
        begin_ = container_->cbegin();
        shiftedTooFar_ = false;
    }
    //! shift the internal pointer by amount
    void shift(Container::difference_type amount = 1) {
        begin_ += std::min(amount, std::distance(begin_, container_->cend()));
    }
    Container toVector() const {
        return Container(begin_, container_->cend());
    }
    //! try read a value if possible
    virtual ArgList& operator>>(std::string& val);

    //! tell whether all previous operator>>() succeeded
    operator bool() const {
        return !shiftedTooFar_;
    }

protected:
    //! shift state pointing into container_
    Container::const_iterator begin_;
    //! indicator that we attempted to shift too far (shift is at end())
    bool shiftedTooFar_ = false;
    /*! Argument vector
     * @note This is a shared pointer to make object copy-able:
     * 1. payload is shared (no redundant copies)
     * 2. begin_ stays valid
     * 3. The C-style compatibility layer DEPENDS on the shared_ptr!
     */
    std::shared_ptr<Container> container_;
};

#endif
