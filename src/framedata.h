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

#include <vector>
#include <memory>

class Client;

class FrameDataLeaf {
public:
    std::vector<Client*> clients;
    int     selection = 0;
    int     layout = 0;
};

template<typename BaseClass>
class FrameDataSplit {
public:
    int align_ = 0;         // ALIGN_VERTICAL or ALIGN_HORIZONTAL
    std::shared_ptr<BaseClass> a_; // first child
    std::shared_ptr<BaseClass> b_; // second child

    int selection_ = 0;
    int fraction_ = 0; // size of first child relative to whole size
                  // FRACTION_UNIT means full size
                  // FRACTION_UNIT/2 means 50%
};
