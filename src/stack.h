/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBST_STACK_H_
#define __HERBST_STACK_H_

#include <X11/Xlib.h>
#include <glib.h>
#include <stdbool.h>

typedef enum Layer {
    /* layers on each monitor, from top to bottom */
    LAYER_FOCUS,
    LAYER_FULLSCREEN,
    LAYER_NORMAL,
    LAYER_FRAMES,
    LAYER_COUNT,
} HSLayer;

typedef enum SliceType {
    SLICE_CLIENT,
    SLICE_WINDOW,
    SLICE_MONITOR,
} HSSliceType;

struct HSClient;
struct HSMonitor;
struct GList;

typedef struct HSSlice {
    HSSliceType type;
    HSLayer     layer[LAYER_COUNT]; /* layers this slice is contained in */
    size_t      layer_count;        /* count of those layers */
    union {
        struct HSClient*    client;
        Window              window;
        struct HSMonitor*   monitor;
    } data;
} HSSlice;

typedef struct HSStack {
    GList*  top[LAYER_COUNT];
    bool    dirty;  /* stacking order changed but it wasn't restacked yet */
} HSStack;


void stacklist_init();
void stacklist_destroy();

HSSlice* slice_create_window(Window window);
HSSlice* slice_create_frame(Window window);
HSSlice* slice_create_client(struct HSClient* client);
HSSlice* slice_create_monitor(struct HSMonitor* monitor);
void slice_destroy(HSSlice* slice);
HSLayer slice_highest_layer(HSSlice* slice);

void stack_insert_slice(HSStack* s, HSSlice* elem);
void stack_remove_slice(HSStack* s, HSSlice* elem);
void stack_raise_slide(HSStack* stack, HSSlice* slice);
void stack_mark_dirty(HSStack* s);
void stack_slice_add_layer(HSStack* s, HSSlice* slice, HSLayer layer);
void stack_slice_remove_layer(HSStack* s, HSSlice* slice, HSLayer layer);
bool stack_is_layer_empty(HSStack* s, HSLayer layer);
void stack_clear_layer(HSStack* s, HSLayer layer);

int print_stack_command(int argc, char** argv, GString** result);

// returns the number of windows in this stack
int stack_window_count(HSStack* stack);
void stack_to_window_buf(HSStack* stack, Window* buf, int len, int* remain_len);
void stack_restack(HSStack* stack);
Window stack_lowest_window(HSStack* stack);

HSStack* stack_create();
void stack_destroy(HSStack* s);



#endif

