#ifndef __HERBST_STACK_H_
#define __HERBST_STACK_H_

#include <X11/X.h>
#include <array>
#include <functional>
#include <set>
#include <string>

#include "plainstack.h"

enum HSLayer {
    /* layers on each tag, from top to bottom */
    LAYER_FOCUS,
    LAYER_FULLSCREEN,
    LAYER_FLOATING,
    LAYER_NORMAL,
    LAYER_FRAMES,
    LAYER_COUNT,
};

extern const std::array<const char*, LAYER_COUNT> g_layer_names;

class Client;

class Slice {
public:
    Slice();
    ~Slice() = default;

    enum class Type {
        ClientSlice,
        WindowSlice,
    };

    static Slice* makeWindowSlice(Window window);
    static Slice* makeFrameSlice(Window window);
    static Slice* makeClientSlice(Client* client);

    std::string getLabel();
    void extractWindowsFromSlice(bool real_clients, HSLayer layer,
                                 std::function<void(Window)> yield);

    std::set<HSLayer> layers; //!< layers this slice is contained in
private:
    HSLayer highestLayer() const;

    Type type = {};
    union {
        Client*    client;
        Window              window;
    } data = {};
};

class Stack {
public:
    Stack() = default;
    ~Stack();

    void insertSlice(Slice* elem);
    void removeSlice(Slice* elem);
    void raiseSlice(Slice* slice);
    void sliceAddLayer(Slice* slice, HSLayer layer, bool insertOnTop = true);
    void sliceRemoveLayer(Slice* slice, HSLayer layer);
    bool isLayerEmpty(HSLayer layer);
    void clearLayer(HSLayer layer);

    void extractWindows(bool real_clients, std::function<void(Window)> yield);

    PlainStack<Slice*> layers_[LAYER_COUNT];

private:
    //! Whether the stacking order has changed but wasn't restacked yet
    bool dirty = false;
};

#endif

