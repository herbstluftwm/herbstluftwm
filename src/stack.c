#include "stack.h"

#include "clientlist.h"
#include <glib.h>

void stacklist_init() {
}

void stacklist_destroy() {
}


HSStack* stack_create() {
    return g_new0(HSStack, 1);
}

void stack_destroy(HSStack* s) {
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

HSSlice* slice_create_client(HSClient* client) {
    HSSlice* s = slice_create();
    s->type = SLICE_CLIENT;
    s->data.client = client;
    return s;
}

void slice_destroy(HSSlice* slice) {
    g_free(slice);
}


