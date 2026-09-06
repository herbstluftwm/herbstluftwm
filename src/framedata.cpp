#include "framedata.h"

#include "completion.h"

using std::string;

template<> Finite<SplitAlign>::ValueList Finite<SplitAlign>::values = ValueListPlain {
    { SplitAlign::vertical, "vertical" },
    { SplitAlign::horizontal, "horizontal" },
};

template<> Finite<LayoutAlgorithm>::ValueList Finite<LayoutAlgorithm>::values = {
  { FiniteNameFlags::AllowIndicesAsNames },
  {
    { LayoutAlgorithm::vertical, "vertical" },
    { LayoutAlgorithm::horizontal, "horizontal" },
    { LayoutAlgorithm::max, "max" },
    { LayoutAlgorithm::grid, "grid" },
    { LayoutAlgorithm::masterstack, "masterstack" },
  }
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

template<> Finite<DirectionLevel>::ValueList Finite<DirectionLevel>::values = ValueListPlain {
    { DirectionLevel::Frame, "frame" },
    { DirectionLevel::Visible, "visible" },
    { DirectionLevel::Tabs, "tabs" },
    { DirectionLevel::All, "all" }
};
