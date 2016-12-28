#ifndef CHILDBYINDEX_H
#define CHILDBYINDEX_H

#include <memory>
#include <unordered_map>
#include <string>
#include "object.h"



/** an object that carries a vector of children objects, each accessible by its
 * index
 */
template<typename T>
class ChildByIndex : public Object {
public:
    ChildByIndex(const std::string& name) : Object(name) {}
    void addIndexed(std::shared_ptr<T> newChild) {
        unsigned long index_int = data.size();
        std::string index = std::to_string(index_int);
        data.push_back(newChild);
        // add a child object
        addChild(newChild, index);
        newChild->setIndexAttribute(index_int);
    }
    ~ChildByIndex() {
        clearChildren();
    }

    void removeIndexed(size_t idx) {
        auto remove_it = data.begin() + idx;
        if (idx < 0 || remove_it == data.end()) {
            return;
        }
        // remove that value
        removeChild(std::to_string(idx));
        data.erase(remove_it);
        for (size_t new_idx = idx; new_idx < data.size(); new_idx++) {
            std::string old_idx_str = std::to_string(new_idx + 1);
            removeChild(old_idx_str);
            addChild(data[new_idx], std::to_string(new_idx));
        }
    }

    int index_of(T* child) {
        for (int i = 0; i < data.size(); i++) {
            if (&* data[i] == child) {
                return i;
            }
        }
        return -1;
    }

    T& operator[](size_t idx) {
        return *data[idx];
    }

    std::shared_ptr<T> byIdx(size_t idx) {
        return data[idx];
    }

    size_t size() {
        return data.size();
    }

    // remove all "indexed" children
    void clearChildren() {
        for (size_t idx = 0; idx < data.size(); idx++) {
            removeChild(std::to_string(idx));
        }
        data.erase(data.begin(), data.end());
    }


    // iterators
    typedef typename std::vector<std::shared_ptr<T>>::iterator iterator_type;
    iterator_type begin() { return data.begin(); }
    iterator_type end() { return data.end(); }
private:
    std::vector<std::shared_ptr<T>> data;
};


#endif
