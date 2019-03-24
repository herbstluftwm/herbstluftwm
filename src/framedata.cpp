#include "framedata.h"

#include "layout.h"

size_t LayoutAlgorithmCount() {
    size_t i = 0;
    while (g_layout_names[i] != nullptr) {
        i++;
    }
    return i;
}
