#include "framedata.h"

#include "completion.h"

using std::string;

template<> Finite<SplitAlign>::ValueList Finite<SplitAlign>::values = {
    { SplitAlign::vertical, "vertical" },
    { SplitAlign::horizontal, "horizontal" },
};

template<> Finite<LayoutAlgorithm>::ValueList Finite<LayoutAlgorithm>::values = {
    { LayoutAlgorithm::vertical, "vertical" },
    { LayoutAlgorithm::horizontal, "horizontal" },
    { LayoutAlgorithm::max, "max" },
    { LayoutAlgorithm::grid, "grid" },
};

size_t layoutAlgorithmCount() {
    return Finite<LayoutAlgorithm>::values.size();
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
