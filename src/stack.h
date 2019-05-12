#ifndef __HERBST_STACK_H_
#define __HERBST_STACK_H_

#include <X11/X.h>
#include <array>
#include <functional>
#include <set>

#include "plainstack.h"

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

class Slice {
public:
    Slice();
    ~Slice() = default;

    static Slice* makeWindowSlice(Window window);
    static Slice* makeFrameSlice(Window window);
    static Slice* makeClientSlice(Client* client);

    std::string getLabel();

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

    PlainStack<Slice*> layers_[LAYER_COUNT];

private:
    bool    dirty;  /* stacking order changed but it wasn't restacked yet */
};

HSLayer slice_highest_layer(Slice* slice);

#endif

