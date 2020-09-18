#include "framedata.h"

#include "completion.h"

using std::string;

static const char* g_align_names[] = {
    "vertical",
    "horizontal",
    nullptr,
};

static const char* g_layout_names[] = {
    "vertical",
    "horizontal",
    "max",
    "grid",
    nullptr,
};

size_t layoutAlgorithmCount() {
    size_t i = 0;
    while (g_layout_names[i] != nullptr) {
        i++;
    }
    return i;
}

template<> LayoutAlgorithm Converter<LayoutAlgorithm>::parse(const string& source) {
    for (size_t i = 0; g_layout_names[i] != nullptr; i++) {
        if (source == g_layout_names[i]) {
            return (LayoutAlgorithm) i;
        }
    }
    throw std::invalid_argument("Invalid layout name: \"" + source + "\"");
}

template<> string Converter<LayoutAlgorithm>::str(LayoutAlgorithm payload) {
    return g_layout_names[(int) payload];
}

template<> void Converter<LayoutAlgorithm>::complete(Completion& complete, LayoutAlgorithm const* relativeTo) {
    for (size_t i = 0; g_layout_names[i] != nullptr; i++) {
        complete.full(g_layout_names[i]);
    }
}

template<> SplitAlign Converter<SplitAlign>::parse(const string& source) {
    for (size_t i = 0; g_align_names[i] != nullptr; i++) {
        if (source == g_align_names[i]) {
            return (SplitAlign) i;
        }
    }
    throw std::invalid_argument("Invalid split align name: \"" + source + "\"");
}

template<> string Converter<SplitAlign>::str(SplitAlign payload) {
    return g_align_names[(int) payload];
}

template<> void Converter<SplitAlign>::complete(Completion& complete, SplitAlign const* relativeTo) {
    for (size_t i = 0; g_align_names[i] != nullptr; i++) {
        complete.full(g_align_names[i]);
    }
}

//! embed the SplitAlign type into the LayoutAlgorithm type
LayoutAlgorithm splitAlignToLayoutAlgorithm(SplitAlign align)
{
    switch (align) {
        case SplitAlign::vertical:
            return LayoutAlgorithm::vertical;
        case SplitAlign::horizontal:
            return LayoutAlgorithm::horizontal;
    }
    return LayoutAlgorithm::vertical;
}
