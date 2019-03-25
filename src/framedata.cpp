#include "framedata.h"

#include "completion.h"
#include "layout.h"

using std::string;

size_t LayoutAlgorithmCount() {
    size_t i = 0;
    while (g_layout_names[i] != nullptr) {
        i++;
    }
    return i;
}

template<> SplitAlign Converter<SplitAlign>::parse(const std::string& source, SplitAlign const* relativeTo) {
    for (size_t i = 0; g_align_names[i] != nullptr; i++) {
        if (source == g_align_names[i]) {
            return (SplitAlign) i;
        }
    }
    throw std::invalid_argument("Invalid split align name: \"" + source + "\"");
}

template<> string Converter<SplitAlign>::str(SplitAlign payload) {
    return string(g_align_names[(int)payload]);
}

template<> void Converter<SplitAlign>::complete(Completion& complete, SplitAlign const* relativeTo) {
    for (size_t i = 0; g_align_names[i] != nullptr; i++) {
        complete.full(g_align_names[i]);
    }
}
