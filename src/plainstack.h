#pragma once

#include <algorithm>
#include <cassert>
#include <vector>

template<typename T>
class PlainStack {
public:
    //! insert at the top
    void insert(const T& element) {
        data_.insert(data_.begin(), element);
    }
    void remove(const T& element) {
        data_.erase(std::remove(data_.begin(), data_.end(), element), data_.end());
    }
    void raise(const T& element) {
        auto it = std::find(data_.begin(), data_.end(), element);
        assert(it != data_.end());
        // rotate the range [begin, it+1) in such a way
        // that it becomes the new first element
        std::rotate(data_.begin(), it, it + 1);
    }
    typename std::vector<T>::const_iterator begin() const {
        return data_.cbegin();
    }
    typename std::vector<T>::const_iterator end() const {
        return data_.cend();
    }
    typename std::vector<T>::const_reverse_iterator rbegin() const {
        return data_.rbegin();
    }
    typename std::vector<T>::const_reverse_iterator rend() const {
        return data_.rend();
    }
    bool empty() const {
        return data_.empty();
    }
private:
    std::vector<T> data_;
};

