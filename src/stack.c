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

static void slice_print(HSSlice* slice, GString** result) {
    switch (slice->type) {
        case SLICE_WINDOW:
            g_string_append_printf(*result, "  :: %lx Window",
                                   slice->data.window);
            break;
        case SLICE_CLIENT:
            g_string_append_printf(*result, "  :: %lx Client \"%s\"",
                                   slice->data.client->window,
                                   slice->data.client->title->str);
            break;
        case SLICE_MONITOR:
            g_string_append_printf(*result, "  :: Monitor %d",
                                   monitor_index_of(slice->data.monitor));
    }
    *result = g_string_append_c(*result, '\n');
}

int print_stack_command(int argc, char** argv, GString** result) {
    HSTag* tag = get_current_monitor()->tag;
    HSStack* stack = tag->stack;
    for (int i = 0; i < LAYER_COUNT; i++) {
        g_string_append_printf(*result, "==> Layer %d\n", i);
        g_list_foreach(stack->top[i], (GFunc)slice_print, result);
    }
}

int stack_window_count(HSStack* stack) {
    int counter = 0;
    stack_to_window_buf(stack, NULL, 0, &counter);
    return -counter;
}

/* stack to window buf */
struct s2wb {
    int     len;
    Window* buf;
    int     missing; /* number of slices that could not find space in buf */
};

static void slice_to_window_buf(HSSlice* s, struct s2wb* data) {
    HSTag* tag;
    switch (s->type) {
        case SLICE_CLIENT:
            if (data->len) {
                data->buf[0] = s->data.client->window;
                data->buf++;
                data->len--;
            } else {
                data->missing++;
            }
            break;
        case SLICE_WINDOW:
            if (data->len) {
                data->buf[0] = s->data.window;
                data->buf++;
                data->len--;
            } else {
                data->missing++;
            }
            break;
        case SLICE_MONITOR:
            tag = s->data.monitor->tag;
            int remain_len = 0; /* remaining length */
            stack_to_window_buf(tag->stack, data->buf, data->len, &remain_len);
            int len_used = data->len - remain_len;
            if (remain_len > 0) {
                data->buf += len_used;
                data->len -= len_used;
            } else {
                data->buf += data->len;
                data->len = 0;
                data->missing = -remain_len;
            }
            break;
    }
}

void stack_to_window_buf(HSStack* stack, Window* buf, int len, int* remain_len) {
    struct s2wb data = {
        .len = len,
        .buf = buf,
        .missing = 0,
    };
    for (int i = 0; i < LAYER_COUNT; i++) {
        g_list_foreach(stack->top[i], (GFunc)slice_to_window_buf, &data);
    }
    if (!remain_len) {
        // nothing to do
        return;
    }
    if (data.missing == 0) {
        *remain_len = len - data.len;
    } else {
        *remain_len = -data.missing;
    }
}

