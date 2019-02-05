#include "frametree.h"

#include "layout.h"

FrameTree::FrameTree(HSTag* tag, Settings* settings)
    : tag_(tag)
    , settings_(settings)
{
    root_ = std::make_shared<HSFrameLeaf>(tag, settings, std::shared_ptr<HSFrameSplit>());
    (void) tag_;
    (void) settings_;
}

void FrameTree::foreachClient(std::function<void(Client*)> action)
{
    root_->foreachClient(action);
}
