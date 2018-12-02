#ifndef __HERBST_STACK_H_
#define __HERBST_STACK_H_

#include <X11/Xlib.h>
#include "glib-backports.h"
#include <array>
#include <memory>
#include "x11-types.h"
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
} HSSliceType;

class HSClient;
class HSMonitor;

typedef struct HSSlice {
    HSSliceType type;
    std::set<HSLayer> layers; //!< layers this slice is contained in
    union {
        HSClient*    client;
        Window              window;
        HSMonitor*          monitor;
    } data;
} HSSlice;

class HSStack {
public:
    HSStack() = default;
    ~HSStack();

    void insert_slice(HSSlice* elem);
    void remove_slice(HSSlice* elem);
    void raise_slide(HSSlice* slice);
    void mark_dirty();
    void slice_add_layer(HSSlice* slice, HSLayer layer);
    void slice_remove_layer(HSSlice* slice, HSLayer layer);
    bool is_layer_empty(HSLayer layer);
    void clear_layer(HSLayer layer);

    // returns the number of windows in this stack
    int window_count(bool real_clients);
    void to_window_buf(Window* buf, int len, bool real_clients, int* remain_len);
    void restack();
    Window lowest_window();

    GList*  top[LAYER_COUNT];

private:
    bool    dirty;  /* stacking order changed but it wasn't restacked yet */
};

void stacklist_init();
void stacklist_destroy();

HSSlice* slice_create_window(Window window);
HSSlice* slice_create_frame(Window window);
HSSlice* slice_create_client(HSClient* client);
HSSlice* slice_create_monitor(HSMonitor* monitor);
void slice_destroy(HSSlice* slice);
HSLayer slice_highest_layer(HSSlice* slice);

int print_stack_command(int argc, char** argv, Output output);

#endif

