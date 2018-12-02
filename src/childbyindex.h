#ifndef CHILDBYINDEX_H
#define CHILDBYINDEX_H

#include <unordered_map>
#include <string>
#include "object.h"
#include "attribute_.h"



/** an object that carries a vector of children objects, each accessible by its
 * index
 */
template<typename T>
class ChildByIndex : public Object {
public:
    ChildByIndex()
    : count("count", [this]() { return this->size(); })
    { wireAttributes({ &count }); }
    void addIndexed(T* newChild) {
        unsigned long index_int = data.size();
        std::string index = std::to_string(index_int);
        data.push_back(newChild);
        // add a child object
        addChild(newChild, index);
        newChild->setIndexAttribute(index_int);
    }
    ~ChildByIndex() override {
        clearChildren();
    }

    void removeIndexed(size_t idx) {
        if (idx >= data.size()) {
            // index does not exist
            return;
        }

        T* child = byIdx(idx);
        data.erase(data.begin() + idx);

        removeChild(std::to_string(idx));

        // Update indices for remaining children
        for (size_t new_idx = idx; new_idx < data.size(); new_idx++) {
            std::string old_idx_str = std::to_string(new_idx + 1);
            removeChild(old_idx_str);
            addChild(data[new_idx], std::to_string(new_idx));
        }

        delete child;
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

    T* byIdx(size_t idx) {
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
        for (auto child : data) {
            delete child;
        }
        data.erase(data.begin(), data.end());
    }

    DynAttribute_<unsigned long> count;

    // iterators
    typedef typename std::vector<T*>::iterator iterator_type;
    iterator_type begin() { return data.begin(); }
    iterator_type end() { return data.end(); }
private:
    std::vector<T*> data;
};


#endif
