#include "stack.h"

#include <X11/Xlib.h>
#include <algorithm>
#include <cassert>
#include <cstdio>

#include "client.h"
#include "ewmh.h"
#include "globals.h"
#include "monitor.h"
#include "monitormanager.h"
#include "plainstack.h"
#include "tag.h"
#include "utils.h"

using std::function;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;

const std::array<const char*, LAYER_COUNT>g_layer_names =
    ArrayInitializer<const char*, LAYER_COUNT>({
     { LAYER_FOCUS,        "Focus-Layer"      },
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

Slice::Slice() {
    layers.insert(LAYER_NORMAL);
}

Slice* Slice::makeWindowSlice(Window window) {
    auto s = new Slice();
    s->type = SLICE_WINDOW;
    s->data.window = window;
    return s;
}

Slice* Slice::makeFrameSlice(Window window) {
    auto s = Slice::makeWindowSlice(window);
    s->layers.clear();
    s->layers.insert(LAYER_FRAMES);
    return s;
}


Slice* Slice::makeClientSlice(Client* client) {
    auto s = new Slice();
    s->type = SLICE_CLIENT;
    s->data.client = client;
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

void Stack::insertSlice(Slice* elem) {
    for (auto layer : elem->layers) {
        top[layer].insert(top[layer].begin(), elem);
    }
    dirty = true;
}

void Stack::removeSlice(Slice* elem) {
    for (auto layer : elem->layers) {
        auto &v = top[layer];
        v.erase(std::remove(v.begin(), v.end(), elem));
    }
    dirty = true;
}

static string getSliceLabel(const Slice* slice) {
    std::stringstream label;
    switch (slice->type) {
        case SLICE_WINDOW:
            label << "Window 0x" << std::hex << slice->data.window << std::dec;
            break;
        case SLICE_CLIENT:
            label << "Client 0x"
                  << std::hex << slice->data.client->x11Window() << std::dec
                  << " \"" << slice->data.client->title_() << "\"";
            break;
        default: ;
    }
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

    shared_ptr<TreeInterface> nthChild(size_t idx) override {
        return children_.at(idx);
    };

    void appendCaption(Output output) override {
        if (label_ != "") {
            output << " " << label_;
        }
    };

private:
    vector<shared_ptr<StringTree>> children_;
    string label_;
};

int print_stack_command(int argc, char** argv, Output output) {
    vector<shared_ptr<StringTree>> monitors;
    for (Monitor* monitor : g_monitors->monitorStack_) {
        vector<shared_ptr<StringTree>> layers;
        for (size_t layerIdx = 0; layerIdx < LAYER_COUNT; layerIdx++) {
            auto layer = monitor->tag->stack->top[layerIdx];

            vector<shared_ptr<StringTree>> slices;
            for (auto& slice : layer) {
                slices.push_back(make_shared<StringTree>(getSliceLabel(slice)));
            }

            auto layerLabel = g_layer_names[layerIdx];
            layers.push_back(make_shared<StringTree>(layerLabel, slices));
        }

        monitors.push_back(make_shared<StringTree>(monitor->getDescription(), layers));
    }

    auto stackRoot = make_shared<StringTree>("", monitors);
    tree_print_to(stackRoot, output);
    return 0;
}

//! helper function for Stack::toWindowBuf() for a given Slice and layer. The
//other parameters are as for Stack::toWindowBuf()
static void extractWindowsFromSlice(Slice* s, bool real_clients, HSLayer layer,
                                function<void(Window)> yield) {
    if (slice_highest_layer(s) != layer) {
        /** slice only is added to its highest layer.
         * just skip it if the slice is not shown on this data->layer */
        return;
    }
    switch (s->type) {
        case SLICE_CLIENT:
            if (real_clients) {
                yield(s->data.client->x11Window());
            } else {
                yield(s->data.client->decorationWindow());
            }
            break;
        case SLICE_WINDOW:
            if (!real_clients) {
                yield(s->data.window);
            }
            break;
    }
}

//! return the stack of windows by successive calls to the given yield
//function. The stack is returned from top to bottom, i.e. the topmost element
//is the first element added to the stack.
void Stack::extractWindows(bool real_clients, function<void(Window)> yield) {
    for (int i = 0; i < LAYER_COUNT; i++) {
        for (auto slice : top[i]) {
            extractWindowsFromSlice(slice, real_clients, (HSLayer)i, yield);
        }
    }
}

void Stack::restack() {
    if (!dirty) {
        return;
    }
    vector<Window> buf;
    extractWindows(false, [&buf](Window w) { buf.push_back(w); });
    XRestackWindows(g_display, buf.data(), buf.size());
    dirty = false;
    Ewmh::get().updateClientListStacking();
}

void Stack::raiseSlice(Slice* slice) {
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

void Stack::markDirty() {
    dirty = true;
}

void Stack::sliceAddLayer(Slice* slice, HSLayer layer) {
    if (slice->layers.count(layer) != 0) {
        /* nothing to do */
        return;
    }

    slice->layers.insert(layer);
    top[layer].insert(top[layer].begin(), slice);
    dirty = true;
}

void Stack::sliceRemoveLayer(Slice* slice, HSLayer layer) {
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

Window Stack::lowestWindow() {
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
            }
            if (w) {
                return w;
            }
        }
    }
    // if no window was found
    return 0;
}

bool Stack::isLayerEmpty(HSLayer layer) {
    return top[layer].empty();
}

void Stack::clearLayer(HSLayer layer) {
    while (!isLayerEmpty(layer)) {
        sliceRemoveLayer(top[layer].front(), layer);
        dirty = true;
    }
}

