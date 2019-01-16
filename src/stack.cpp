#include "stack.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iomanip>

#include "client.h"
#include "ewmh.h"
#include "globals.h"
#include "monitor.h"
#include "tag.h"
#include "utils.h"

using std::vector;
using std::shared_ptr;
using std::make_shared;
using std::string;

const std::array<const char*, LAYER_COUNT>g_layer_names =
    ArrayInitializer<const char*, LAYER_COUNT>({
     { LAYER_FOCUS	 , "Focus-Layer"      },
     { LAYER_FULLSCREEN  , "Fullscreen-Layer" },
     { LAYER_NORMAL      , "Normal Layer"     },
     { LAYER_FRAMES      , "Frame Layer"      },
}).a;


Stack::~Stack() {
    for (int i = 0; i < LAYER_COUNT; i++) {
        if (!top[i].empty()) {
            HSDebug("Warning: %s of stack %p was not empty on destroy\n",
                    g_layer_names[i], (void*)this);
        }
    }
}

static Slice* slice_create() {
    Slice* s = new Slice();
    s->layers.insert(LAYER_NORMAL);
    return s;
}

Slice* slice_create_window(Window window) {
    Slice* s = slice_create();
    s->type = SLICE_WINDOW;
    s->data.window = window;
    return s;
}

Slice* slice_create_frame(Window window) {
    Slice* s = slice_create_window(window);
    s->layers.clear();
    s->layers.insert(LAYER_FRAMES);
    return s;
}


Slice* slice_create_client(HSClient* client) {
    Slice* s = slice_create();
    s->type = SLICE_CLIENT;
    s->data.client = client;
    return s;
}

Slice* slice_create_monitor(Monitor* monitor) {
    Slice* s = slice_create();
    s->type = SLICE_MONITOR;
    s->data.monitor = monitor;
    return s;
}

void slice_destroy(Slice* slice) {
    delete slice;
}

HSLayer slice_highest_layer(Slice* slice) {
    if (slice->layers.empty()) {
        return LAYER_COUNT;
    } else {
        return *(slice->layers.begin());
    }
}

void Stack::insert_slice(Slice* elem) {
    for (auto layer : elem->layers) {
        top[layer].insert(top[layer].begin(), elem);
    }
    dirty = true;
}

void Stack::remove_slice(Slice* elem) {
    for (auto layer : elem->layers) {
        auto &v = top[layer];
        v.erase(std::remove(v.begin(), v.end(), elem));
    }
    dirty = true;
}

static void slice_append_caption(HSTree root, Output output) {
    Slice* slice = (Slice*)root;
    GString* monitor_name = g_string_new("");
    switch (slice->type) {
        case SLICE_WINDOW:
            output << "Window 0x" << std::hex << slice->data.window << std::dec;
            break;
        case SLICE_CLIENT:
            output << "Client 0x"
                   << std::hex << slice->data.client->x11Window() << std::dec
                   << " \"" << slice->data.client->title_() << "\"";
            break;
        case SLICE_MONITOR:
            if (slice->data.monitor->name != "") {
                g_string_append_printf(monitor_name, " (\"%s\")",
                                       slice->data.monitor->name->c_str());
            }
            output << "Monitor "
                   << slice->data.monitor->index()
                   << monitor_name->str
                   << " with tag \""
                   << *(slice->data.monitor->tag->name)
                   << "\"";
            break;
    }
    g_string_free(monitor_name, true);
}

static string getMonitorLabel(const Monitor* monitor) {
    std::stringstream label;
    label << "Monitor "
        << monitor->index()
        << monitor->name()
        << " with tag \""
        << monitor->tag->name()
        << "\"";
    return label.str();
}

class StringTree : public TreeInterface {
public:
    StringTree(string label, vector<shared_ptr<StringTree>> children = {})
        : children_(children)
        , label_(label)
    {};

    size_t childCount() override {
        return children_.size();
    };

    Ptr(TreeInterface) nthChild(size_t idx) override {
        return children_.at(idx);
    };

    void appendCaption(Output output) override {
        output << label_;
    };

private:
    vector<shared_ptr<StringTree>> children_;
    string label_;
};

int print_stack_command(int argc, char** argv, Output output) {
    auto monitorStack = get_monitor_stack();
    vector<shared_ptr<StringTree>> monitors;
    for (auto& monitorSlice : monitorStack->top[LAYER_NORMAL]) {
        auto monitor = monitorSlice->data.monitor;
        vector<shared_ptr<StringTree>> layers;
        for (size_t layerIdx = 0; layerIdx < LAYER_COUNT; layerIdx++) {
            auto layer = monitor->tag->stack->top[layerIdx];

            vector<shared_ptr<StringTree>> slices;
            for (auto& slice : layer) {
                std::stringstream childLabel;
                slice_append_caption(slice, childLabel);
                slices.push_back(make_shared<StringTree>(childLabel.str()));
            }

            auto layerLabel = g_layer_names[layerIdx];
            layers.push_back(make_shared<StringTree>(layerLabel, slices));
        }

        auto monitorLabel = getMonitorLabel(monitor);
        monitors.push_back(make_shared<StringTree>(monitorLabel, layers));
    }

    auto stackRoot = make_shared<StringTree>("", monitors);
    tree_print_to(stackRoot, output);
    return 0;
}

int Stack::window_count(bool real_clients) {
    int counter = 0;
    to_window_buf(nullptr, 0, real_clients, &counter);
    return -counter;
}

/* stack to window buf */
struct s2wb {
    int     len;
    Window* buf;
    int     missing; /* number of slices that could not find space in buf */
    bool    real_clients; /* whether to include windows that aren't clients */
    HSLayer layer;  /* the layer the slice should be added to */
};

static void slice_to_window_buf(Slice* s, struct s2wb* data) {
    HSTag* tag;
    if (slice_highest_layer(s) != data->layer) {
        /** slice only is added to its highest layer.
         * just skip it if the slice is not shown on this data->layer */
        return;
    }
    switch (s->type) {
        case SLICE_CLIENT:
            if (data->len) {
                if (data->real_clients) {
                    data->buf[0] = s->data.client->x11Window();
                } else {
                    data->buf[0] = s->data.client->decorationWindow();
                }
                data->buf++;
                data->len--;
            } else {
                data->missing++;
            }
            break;
        case SLICE_WINDOW:
            if (!data->real_clients) {
                if (data->len) {
                    data->buf[0] = s->data.window;
                    data->buf++;
                    data->len--;
                } else {
                    data->missing++;
                }
            }
            break;
        case SLICE_MONITOR:
            tag = s->data.monitor->tag;
            if (!data->real_clients) {
                if (data->len) {
                    data->buf[0] = s->data.monitor->stacking_window;
                    data->buf++;
                    data->len--;
                } else {
                    data->missing++;
                }
            }
            int remain_len = 0; /* remaining length */
            tag->stack->to_window_buf(data->buf, data->len,
                                      data->real_clients, &remain_len);
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

void Stack::to_window_buf(Window* buf, int len,
                         bool real_clients, int* remain_len) {
    struct s2wb data = {};
    data.len = len;
    data.buf = buf;
    data.missing = 0;
    data.real_clients = real_clients;

    for (int i = 0; i < LAYER_COUNT; i++) {
        data.layer = (HSLayer)i;
        for (auto slice : top[i]) {
            slice_to_window_buf(slice, &data);
        }
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

void Stack::restack() {
    if (!dirty) {
        return;
    }
    int count = window_count(false);
    Window* buf = g_new0(Window, count);
    to_window_buf(buf, count, false, nullptr);
    XRestackWindows(g_display, buf, count);
    dirty = false;
    ewmh_update_client_list_stacking();
    g_free(buf);
}

void Stack::raise_slide(Slice* slice) {
    for (auto layer : slice->layers) {
        auto &v = top[layer];
        auto it = std::find(v.begin(), v.end(), slice);
        assert(it != v.end());
        // rotate the range [begin, it+1) in such a way
        // that it becomes the new first element
        std::rotate(v.begin(), it, it + 1);
    }
    dirty = true;
    // TODO: maybe only update the specific range and not the entire stack
    // update
    restack();
}

void Stack::mark_dirty() {
    dirty = true;
}

void Stack::slice_add_layer(Slice* slice, HSLayer layer) {
    if (slice->layers.count(layer) != 0) {
        /* nothing to do */
        return;
    }

    slice->layers.insert(layer);
    top[layer].insert(top[layer].begin(), slice);
    dirty = true;
}

void Stack::slice_remove_layer(Slice* slice, HSLayer layer) {
    /* remove slice from layer in the stack */
    auto &v = top[layer];
    v.erase(std::remove(v.begin(), v.end(), slice));
    dirty = true;

    if (slice->layers.count(layer) == 0) {
        HSDebug("remove layer: slice %p not in %s\n", (void*)slice,
                g_layer_names[layer]);
        return;
    }

    slice->layers.erase(layer);
}

Window Stack::lowest_window() {
    for (int i = LAYER_COUNT - 1; i >= 0; i--) {
        auto &v = top[i];
        for (auto it = v.rbegin(); it != v.rend(); it++) {
            auto slice = *it;
            Window w = 0;
            switch (slice->type) {
                case SLICE_CLIENT:
                    w = slice->data.client->decorationWindow();
                    break;
                case SLICE_WINDOW:
                    w = slice->data.window;
                    break;
                case SLICE_MONITOR:
                    w = slice->data.monitor->tag->stack->lowest_window();
                    break;
            }
            if (w) {
                return w;
            }
        }
    }
    // if no window was found
    return 0;
}

bool Stack::is_layer_empty(HSLayer layer) {
    return top[layer].empty();
}

void Stack::clear_layer(HSLayer layer) {
    while (!is_layer_empty(layer)) {
        slice_remove_layer(top[layer].front(), layer);
        dirty = true;
    }
}

