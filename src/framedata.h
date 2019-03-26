#pragma once

/**
 * The frame data header holds the classes that describe the members used for
 * the actual logic of the frame tree tiling. So e.g. the 'selection' is a
 * member of both of the following classes, whereas HSFrameLeaf::last_rect is
 * not.
 *
 * Also note that the following classes only hold the plain members and does
 * not set up class inheritance!
 */

#include <memory>
#include <vector>

#include "types.h"

#define FRACTION_UNIT 10000

class Client;

enum class SplitAlign {
    vertical = 0,
    horizontal,
};

ConverterInstance(SplitAlign)

enum class LayoutAlgorithm {
    vertical = 0,
    horizontal,
    max,
    grid,
};

ConverterInstance(LayoutAlgorithm)

size_t LayoutAlgorithmCount();

class FrameDataLeaf {
protected:
    std::vector<Client*> clients;
    int selection = 0;
    LayoutAlgorithm layout = LayoutAlgorithm::vertical;
};

template<typename BaseClass>
class FrameDataSplit {
protected:
    SplitAlign align_ = SplitAlign::vertical;         // SplitAlign::vertical or SplitAlign::horizontal
    std::shared_ptr<BaseClass> a_; // first child
    std::shared_ptr<BaseClass> b_; // second child

    int selection_ = 0;
    int fraction_ = 0; // size of first child relative to whole size
                  // FRACTION_UNIT means full size
                  // FRACTION_UNIT/2 means 50%
};
