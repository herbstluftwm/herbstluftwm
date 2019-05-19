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
        if (!layers_[i].empty()) {
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

HSLayer Slice::highestLayer() const {
    if (layers.empty()) {
        return LAYER_COUNT;
    } else {
        return *(layers.begin());
    }
}

void Stack::insertSlice(Slice* elem) {
    for (auto layer : elem->layers) {
        layers_[layer].insert(elem);
    }
    dirty = true;
}

void Stack::removeSlice(Slice* elem) {
    for (auto layer : elem->layers) {
        layers_[layer].remove(elem);
    }
    dirty = true;
}

string Slice::getLabel() {
    std::stringstream label;
    switch (type) {
        case SLICE_WINDOW:
            label << "Window " << WindowID(data.window).str();
            break;
        case SLICE_CLIENT:
            label << "Client " << WindowID(data.client->x11Window()).str()
                  << " \"" << data.client->title_() << "\"";
            break;
        default: ;
    }
    return label.str();
}

//! helper function for Stack::toWindowBuf() for a given Slice and layer. The
//other parameters are as for Stack::toWindowBuf()
void Slice::extractWindowsFromSlice(bool real_clients, HSLayer layer,
                                function<void(Window)> yield) {
    if (highestLayer() != layer) {
        /** slice only is added to its highest layer.
         * just skip it if the slice is not shown on this data->layer */
        return;
    }
    switch (type) {
        case SLICE_CLIENT:
            if (real_clients) {
                yield(data.client->x11Window());
            } else {
                yield(data.client->decorationWindow());
            }
            break;
        case SLICE_WINDOW:
            if (!real_clients) {
                yield(data.window);
            }
            break;
    }
}

//! return the stack of windows by successive calls to the given yield
//function. The stack is returned from top to bottom, i.e. the topmost element
//is the first element added to the stack.
void Stack::extractWindows(bool real_clients, function<void(Window)> yield) {
    for (int i = 0; i < LAYER_COUNT; i++) {
        for (auto slice : layers_[i]) {
            slice->extractWindowsFromSlice(real_clients, (HSLayer)i, yield);
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
        layers_[layer].raise(slice);
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
    layers_[layer].insert(slice);
    dirty = true;
}

void Stack::sliceRemoveLayer(Slice* slice, HSLayer layer) {
    /* remove slice from layer in the stack */
    layers_[layer].remove(slice);
    dirty = true;

    if (slice->layers.count(layer) == 0) {
        HSDebug("remove layer: slice %p not in %s\n", (void*)slice,
                g_layer_names[layer]);
        return;
    }

    slice->layers.erase(layer);
}

bool Stack::isLayerEmpty(HSLayer layer) {
    return layers_[layer].empty();
}

void Stack::clearLayer(HSLayer layer) {
    while (!isLayerEmpty(layer)) {
        sliceRemoveLayer(*layers_[layer].begin(), layer);
        dirty = true;
    }
}

