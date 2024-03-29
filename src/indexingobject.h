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
        addIndexed(newChild, data.size());
    }

    void addIndexed(T* newChild, size_t index) {
        // the current array size is the index for the new child
        data.insert(data.begin() + index, newChild);
        // add a child object at the specified index
        addChild(newChild, std::to_string(index));
        newChild->setIndexAttribute(index);
        // update the index of all the later elements
        for (size_t i = index + 1; i < data.size(); i++) {
            addChild(data[i], std::to_string(i));
            data[i]->setIndexAttribute(i);
        }
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

    void indexChangeRequested(T* object, size_t newIndex) {
        if (newIndex < data.size() && data[newIndex] == object) {
            // nothing to do
            return;
        }
        if (data.empty()) {
            return;
        }
        if (newIndex >= data.size()) {
            newIndex = data.size() - 1;
        }
        int oldIndexSigned = index_of(object);
        if (oldIndexSigned < 0) {
            return;
        }
        size_t oldIndex = static_cast<size_t>(oldIndexSigned);
        if (newIndex == oldIndex) {
            return;
        }
        // go from newIndex to oldIndex
        int delta = (newIndex < oldIndex) ? 1 : -1;
        T* lastValue = object;
        for (size_t i = newIndex; i != oldIndex; i+= delta) {
            // swap data[i] with lastValue
            T* tmp = data[i];
            data[i] = lastValue;
            lastValue = tmp;
        }
        data[oldIndex] = lastValue;
        // for each of these, update the index
        for (size_t i = newIndex; i != oldIndex; i+= delta) {
            data[i]->setIndexAttribute(i);
            addChild(data[i], std::to_string(i));
        }
        data[oldIndex]->setIndexAttribute(oldIndex);
        addChild(data[oldIndex], std::to_string(oldIndex));
        indicesChanged.emit();
    }

    DynAttribute_<unsigned long> count;
    Signal indicesChanged;

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
