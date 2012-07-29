#include "stack.h"

#include "clientlist.h"
#include "ewmh.h"
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

HSSlice* slice_create_monitor(HSMonitor* monitor) {
    HSSlice* s = slice_create();
    s->type = SLICE_MONITOR;
    s->data.monitor = monitor;
    return s;
}

void slice_destroy(HSSlice* slice) {
    g_free(slice);
}

void stack_insert_slice(HSStack* s, HSSlice* elem) {
    int layer = elem->layer;
    s->top[layer] = g_list_append(s->top[layer], elem);
    s->dirty = true;
    HSDebug("stack %p += %p, layer = %d\n", (void*)s, (void*)elem, elem->layer);
}

void stack_remove_slice(HSStack* s, HSSlice* elem) {
    int layer = elem->layer;
    s->top[layer] = g_list_remove(s->top[layer], elem);
    s->dirty = true;
    HSDebug("stack %p -= %p, layer = %d\n", (void*)s, (void*)elem, elem->layer);
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
    // remove slice from list
    stack->top[slice->layer] = g_list_remove(stack->top[slice->layer], slice);
    // and insert it again at the top
    stack->top[slice->layer] = g_list_prepend(stack->top[slice->layer], slice);
    stack->dirty = true;
    // TODO: maybe only update the specific range and not the entire stack
    // update
    stack_restack(stack);
}

void stack_mark_dirty(HSStack* s) {
    s->dirty = true;
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

