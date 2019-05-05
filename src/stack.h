#ifndef __HERBST_STACK_H_
#define __HERBST_STACK_H_

#include <X11/X.h>
#include <array>
#include <functional>
#include <set>
#include <vector>

#include "types.h"

enum HSLayer {
    /* layers on each tag, from top to bottom */
    LAYER_FOCUS,
    LAYER_FULLSCREEN,
    LAYER_NORMAL,
    LAYER_FRAMES,
    LAYER_COUNT,
};

extern const std::array<const char*, LAYER_COUNT> g_layer_names;

typedef enum SliceType {
    SLICE_CLIENT,
    SLICE_WINDOW,
} SliceType;

class Client;

template<typename T>
class PlainStack {
public:
    //! insert at the top
    void insert(const T& element) {
        data_.insert(data_.begin(), element);
    }
    void remove(const T& element) {
        data_.erase(std::remove(data_.begin(), data_.end(), element));
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
private:
    std::vector<T> data_;
};

class Slice {
public:
    Slice();
    ~Slice() = default;

    static Slice* makeWindowSlice(Window window);
    static Slice* makeFrameSlice(Window window);
    static Slice* makeClientSlice(Client* client);

    SliceType type;
    std::set<HSLayer> layers; //!< layers this slice is contained in
    union {
        Client*    client;
        Window              window;
    } data;
};

class Stack {
public:
    Stack() = default;
    ~Stack();

    void insertSlice(Slice* elem);
    void removeSlice(Slice* elem);
    void raiseSlice(Slice* slice);
    void markDirty();
    void sliceAddLayer(Slice* slice, HSLayer layer);
    void sliceRemoveLayer(Slice* slice, HSLayer layer);
    bool isLayerEmpty(HSLayer layer);
    void clearLayer(HSLayer layer);

    void extractWindows(bool real_clients, std::function<void(Window)> addToStack);
    void restack();
    Window lowestWindow();

    std::vector<Slice*> top[LAYER_COUNT];

private:
    bool    dirty;  /* stacking order changed but it wasn't restacked yet */
};

HSLayer slice_highest_layer(Slice* slice);

int print_stack_command(int argc, char** argv, Output output);

#endif

