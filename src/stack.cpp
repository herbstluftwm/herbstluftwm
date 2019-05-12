#include "stack.h"

#include <X11/Xlib.h>

#include "client.h"
#include "ewmh.h"
#include "globals.h"
#include "utils.h"

using std::function;
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
        if (!slices_[i].empty()) {
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
        slices_[layer].insert(elem);
    }
    dirty = true;
}

void Stack::removeSlice(Slice* elem) {
    for (auto layer : elem->layers) {
        slices_[layer].remove(elem);
    }
    dirty = true;
}

string Slice::getLabel() {
    std::stringstream label;
    switch (type) {
        case SLICE_WINDOW:
            label << "Window 0x" << std::hex << data.window << std::dec;
            break;
        case SLICE_CLIENT:
            label << "Client 0x"
                  << std::hex << data.client->x11Window() << std::dec
                  << " \"" << data.client->title_() << "\"";
            break;
        default: ;
    }
    return label.str();
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
        for (auto slice : slices_[i]) {
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
        slices_[layer].raise(slice);
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
    slices_[layer].insert(slice);
    dirty = true;
}

void Stack::sliceRemoveLayer(Slice* slice, HSLayer layer) {
    /* remove slice from layer in the stack */
    slices_[layer].remove(slice);
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
        auto &v = slices_[i];
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
    return slices_[layer].empty();
}

void Stack::clearLayer(HSLayer layer) {
    while (!isLayerEmpty(layer)) {
        sliceRemoveLayer(*slices_[layer].begin(), layer);
        dirty = true;
    }
}

