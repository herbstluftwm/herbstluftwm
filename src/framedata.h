#pragma once

/**
 * The frame data classes hold the members variables that describe the
 * frame tree from a user perspective and as it is described in the 'TILING
 * ALGORITHM' section in the man page. For example, we have
 * FrameDataLeaf::selection and FrameDataSplit<T>::selection_. On the other hand,
 * FrameLeaf::last_rect is more an implementation detail, and thus is a member
 * variable of FrameLeaf and _not_ a member variable of FrameDataLeaf.
 *
 * Also note that FrameDataLeaf and FrameDataSplit do not have a common base
 * class (in contrast to FrameLeaf and FrameSplit which both inherit from
 * Frame).
 */

#include <memory>
#include <vector>

#include "fixprecdec.h"
#include "types.h"

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
template<> void Converter<LayoutAlgorithm>::complete(Completion& complete, LayoutAlgorithm const* relativeTo);

LayoutAlgorithm splitAlignToLayoutAlgorithm(SplitAlign align);

size_t layoutAlgorithmCount();

class FrameDataLeaf {
protected:
    std::vector<Client*> clients;
    int selection = 0;
    LayoutAlgorithm layout = LayoutAlgorithm::vertical;
};

template<typename BaseClass>
class FrameDataSplit {
protected:
    SplitAlign align_ = SplitAlign::vertical;
    std::shared_ptr<BaseClass> a_; // first child
    std::shared_ptr<BaseClass> b_; // second child

    int selection_ = 0;

    /*!
     * Size of first child relative to whole size.
     */
    FixPrecDec fraction_ = FixPrecDec::fromInteger(0);
};
