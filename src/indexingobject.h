#ifndef INDEXINGOBJECT_H
#define INDEXINGOBJECT_H

#include <cstddef>
#include <string>
#include <vector>

#include "attribute_.h"
#include "object.h"

/** an object that carries a vector of children objects, each accessible by its
 * index
 */
template<typename T>
class IndexingObject : public Object {
public:
    IndexingObject()
    : count(this, "count", &IndexingObject<T>::sizeUnsignedLong)
    { }
    void addIndexed(T* newChild) {
        // the current array size is the index for the new child
        unsigned long index = data.size();
        data.push_back(newChild);
        // add a child object
        addChild(newChild, std::to_string(index));
        newChild->setIndexAttribute(index);
    }
    ~IndexingObject() override {
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
            data[new_idx]->setIndexAttribute(new_idx);
        }

        delete child;
    }

    int index_of(T* child) {
        for (size_t i = 0; i < data.size(); i++) {
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
        return idx < data.size() ? data[idx] : nullptr;
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
    unsigned long sizeUnsignedLong() {
        return static_cast<unsigned long>(data.size());
    }

    std::vector<T*> data;
};


#endif
