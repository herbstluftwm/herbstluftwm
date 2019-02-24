#ifndef __HERBST_STACK_H_
#define __HERBST_STACK_H_

#include <X11/X.h>
#include <array>

#include "glib-backports.h"
#include "types.h"

enum HSLayer {
    /* layers on each monitor, from top to bottom */
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
    SLICE_MONITOR,
} SliceType;

class Client;
class Monitor;

class Slice {
public:
    Slice();
    ~Slice() = default;

    static Slice* makeWindowSlice(Window window);
    static Slice* makeFrameSlice(Window window);
    static Slice* makeClientSlice(Client* client);
    static Slice* makeMonitorSlice(Monitor* monitor);

    SliceType type;
    std::set<HSLayer> layers; //!< layers this slice is contained in
    union {
        Client*    client;
        Window              window;
        Monitor*          monitor;
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

    // returns the number of windows in this stack
    int windowCount(bool real_clients);
    void toWindowBuf(Window* buf, int len, bool real_clients, int* remain_len);
    void restack();
    Window lowestWindow();

    std::vector<Slice*> top[LAYER_COUNT];

private:
    bool    dirty;  /* stacking order changed but it wasn't restacked yet */
};

HSLayer slice_highest_layer(Slice* slice);

int print_stack_command(int argc, char** argv, Output output);

#endif

