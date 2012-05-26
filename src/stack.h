/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBST_STACK_H_
#define __HERBST_STACK_H_

#include <X11/Xlib.h>

typedef enum Layer {
    /* layers on each monitor, from top to bottom */
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

typedef struct {
    HSSliceType type;
    HSLayer     layer;
    union {
        struct HSClient*    client;
        Window              window;
        struct HSMonitor*   monitor;
    } data;
} HSSlice;

typedef struct HSStack {
    struct GList*  top;
} HSStack;


void stacklist_init();
void stacklist_destroy();

HSSlice* slice_create_window(Window window);
HSSlice* slice_create_client(struct HSClient* client);
void slice_destroy(HSSlice* slice);

HSStack* stack_create();
void stack_destroy(HSStack* s);



#endif

