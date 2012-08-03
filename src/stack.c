#include "stack.h"

#include "clientlist.h"
#include "ewmh.h"
#include "globals.h"
#include <stdio.h>
#include <string.h>

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
    s->layer[0] = LAYER_NORMAL;
    s->layer_count = 1;
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
    s->layer[0] = LAYER_FRAMES;
    return s;
}


HSSlice* slice_create_client(HSClient* client) {
    HSSlice* s = slice_create();
    s->type = SLICE_CLIENT;
    s->data.client = client;
    return s;
}

HSSlice* slice_create_monitor(HSMonitor* monitor) {
    HSSlice* s = slice_create();
    s->type = SLICE_MONITOR;
    s->data.monitor = monitor;
    return s;
}

void slice_destroy(HSSlice* slice) {
    g_free(slice);
}

HSLayer slice_highest_layer(HSSlice* slice) {
    HSLayer highest = LAYER_COUNT;
    for (int i = 0; i < slice->layer_count; i++) {
        if (slice->layer[i] < highest) {
            highest = slice->layer[i];
        }
    }
    return highest;
}

void stack_insert_slice(HSStack* s, HSSlice* elem) {
    for (int i = 0; i < elem->layer_count; i++) {
        int layer = elem->layer[i];
        s->top[layer] = g_list_append(s->top[layer], elem);
    }
    s->dirty = true;
}

void stack_remove_slice(HSStack* s, HSSlice* elem) {
    for (int i = 0; i < elem->layer_count; i++) {
        int layer = elem->layer[i];
        s->top[layer] = g_list_remove(s->top[layer], elem);
    }
    s->dirty = true;
}

static void stack_print(HSStack* stack, GString** result);

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
            g_string_append_printf(*result, "  :: Monitor %d\n",
                                   monitor_index_of(slice->data.monitor));
            stack_print(slice->data.monitor->tag->stack, result);
            break;
    }
    *result = g_string_append_c(*result, '\n');
}

static void stack_print(HSStack* stack, GString** result) {
    for (int i = 0; i < LAYER_COUNT; i++) {
        g_string_append_printf(*result, "==> Layer %d\n", i);
        g_list_foreach(stack->top[i], (GFunc)slice_print, result);
    }
}

int print_stack_command(int argc, char** argv, GString** result) {
    HSStack* stack = get_monitor_stack();
    stack_print(stack, result);
    stack_mark_dirty(stack);
    stack_restack(stack);
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
    HSLayer layer;  /* the layer the slice should be added to */
};

static void slice_to_window_buf(HSSlice* s, struct s2wb* data) {
    HSTag* tag;
    if (slice_highest_layer(s) != data->layer) {
        /** slice only is added to its highest layer.
         * just skip it if the slice is not shown on this data->layer */
        return;
    }
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
            if (data->len) {
                data->buf[0] = s->data.monitor->stacking_window;
                data->buf++;
                data->len--;
            } else {
                data->missing++;
            }
            int remain_len = 0; /* remaining length */
            stack_to_window_buf(tag->stack, data->buf, data->len, &remain_len);
            int len_used = data->len - remain_len;
            if (remain_len >= 0) {
                data->buf += len_used;
                data->len = remain_len;
            } else {
                data->len = 0;
                data->missing += -remain_len;
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
        data.layer = i;
        g_list_foreach(stack->top[i], (GFunc)slice_to_window_buf, &data);
    }
    if (!remain_len) {
        // nothing to do
        return;
    }
    if (data.missing == 0) {
        *remain_len = data.len;
    } else {
        *remain_len = -data.missing;
    }
}

void stack_restack(HSStack* stack) {
    if (!stack->dirty) {
        return;
    }
    int count = stack_window_count(stack);
    Window* buf = g_new0(Window, count);
    stack_to_window_buf(stack, buf, count, NULL);
    XRestackWindows(g_display, buf, count);
    stack->dirty = false;
    ewmh_update_client_list_stacking();
    g_free(buf);
}

void stack_raise_slide(HSStack* stack, HSSlice* slice) {
    for (int i = 0; i < slice->layer_count; i++) {
        // remove slice from list
        stack->top[slice->layer[i]] = g_list_remove(stack->top[slice->layer[i]], slice);
        // and insert it again at the top
        stack->top[slice->layer[i]] = g_list_prepend(stack->top[slice->layer[i]], slice);
    }
    stack->dirty = true;
    // TODO: maybe only update the specific range and not the entire stack
    // update
    stack_restack(stack);
}

void stack_mark_dirty(HSStack* s) {
    s->dirty = true;
}

void stack_slice_add_layer(HSStack* stack, HSSlice* slice, HSLayer layer) {
    for (int i = 0; i < slice->layer_count; i++) {
        if (slice->layer[i] == layer) {
            /* nothing to do */
            return;
        }
    }
    slice->layer[slice->layer_count] = layer;
    slice->layer_count++;
    stack->top[layer] = g_list_prepend(stack->top[layer], slice);
    stack->dirty = true;
}

void stack_slice_remove_layer(HSStack* stack, HSSlice* slice, HSLayer layer) {
    int i;
    for (i = 0; i < slice->layer_count; i++) {
        if (slice->layer[i] == layer) {
            break;
        }
    }
    if (i >= slice->layer_count) {
        HSDebug("remove layer: slice %p not in layer %d\n", slice, layer);
        return;
    }
    /* remove layer in slice */
    slice->layer_count--;
    size_t len = sizeof(HSLayer) * (slice->layer_count - i);
    memmove(slice->layer + i, slice->layer + i + 1, len);
    /* remove slice from layer in the stack */
    stack->top[layer] = g_list_remove(stack->top[layer], slice);
    stack->dirty = true;
}

Window stack_lowest_window(HSStack* stack) {
    for (int i = LAYER_COUNT - 1; i >= 0; i--) {
        GList* last = g_list_last(stack->top[i]);
        while (last) {
            HSSlice* slice = (HSSlice*)last->data;
            Window w = 0;
            switch (slice->type) {
                case SLICE_CLIENT:
                    w = slice->data.client->window;
                    break;
                case SLICE_WINDOW:
                    w = slice->data.window;
                    break;
                case SLICE_MONITOR:
                    w = stack_lowest_window(slice->data.monitor->tag->stack);
                    break;
            }
            if (w) {
                return w;
            }
            last = g_list_previous(last);
        }
    }
    // if no window was found
    return 0;
}

