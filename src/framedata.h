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

#include "attribute_.h"
#include "converter.h"
#include "finite.h"
#include "fixprecdec.h"

class Client;

enum class SplitAlign {
    vertical = 0,
    horizontal,
};

template <>
struct is_finite<SplitAlign> : std::true_type {};
template<> Finite<SplitAlign>::ValueList Finite<SplitAlign>::values;

template<>
inline Type Attribute_<SplitAlign>::staticType() { return Type::NAMES; }

enum class LayoutAlgorithm {
    vertical = 0,
    horizontal,
    max,
    grid,
};

template <>
struct is_finite<LayoutAlgorithm> : std::true_type {};
template<> Finite<LayoutAlgorithm>::ValueList Finite<LayoutAlgorithm>::values;

template<>
inline Type Attribute_<LayoutAlgorithm>::staticType() { return Type::NAMES; }

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

enum class DirectionLevel { Frame = 0, Visible = 1, Tabs = 2, All = 3 };

template <>
struct is_finite<DirectionLevel> : std::true_type {};
template<> Finite<DirectionLevel>::ValueList Finite<DirectionLevel>::values;


