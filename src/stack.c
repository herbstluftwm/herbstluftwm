#include "stack.h"

#include "clientlist.h"
#include "globals.h"
#include <stdio.h>

void stacklist_init() {
}

void stacklist_destroy() {
}


HSStack* stack_create() {
    return g_new0(HSStack, 1);
}

void stack_destroy(HSStack* s) {
    for (int i = 0; i < LAYER_COUNT; i++) {
        if (s->top[i]) {
            HSDebug("Warning: layer %d of stack %p was not empty on destroy\n",
                    i, (void*)s);
        }
    }
    g_free(s);
}

static HSSlice* slice_create() {
    HSSlice* s = g_new0(HSSlice, 1);
    s->layer = LAYER_NORMAL;
    return s;
}

HSSlice* slice_create_window(Window window) {
    HSSlice* s = slice_create();
    s->type = SLICE_WINDOW;
    s->data.window = window;
    return s;
}

HSSlice* slice_create_frame(Window window) {
    HSSlice* s = slice_create_window(window);
    s->layer = LAYER_FRAMES;
    return s;
}


HSSlice* slice_create_client(HSClient* client) {
    HSSlice* s = slice_create();
    s->type = SLICE_CLIENT;
    s->data.client = client;
    return s;
}

void slice_destroy(HSSlice* slice) {
    g_free(slice);
}

void stack_insert_slice(HSStack* s, HSSlice* elem) {
    int layer = elem->layer;
    s->top[layer] = g_list_append(s->top[layer], elem);
    HSDebug("stack %p += %p, layer = %d\n", (void*)s, (void*)elem, elem->layer);
}

void stack_remove_slice(HSStack* s, HSSlice* elem) {
    int layer = elem->layer;
    s->top[layer] = g_list_remove(s->top[layer], elem);
    HSDebug("stack %p -= %p, layer = %d\n", (void*)s, (void*)elem, elem->layer);
}


