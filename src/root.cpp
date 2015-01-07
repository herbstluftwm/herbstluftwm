#include "root.h"
#include "hookmanager.h"

#include <memory>
#include <stdexcept>

namespace herbstluft {

std::shared_ptr<Root> Root::root_;

std::shared_ptr<Root> Root::create() {
    if (root_)
        throw std::logic_error("Redundant root node creation!");
    root_ = std::make_shared<Root>();
    return root_;
}

void Root::destroy() {
    root_->children_.clear(); // avoid possible circular shared_ptr dependency
    root_.reset();
}


Root::Root() : Directory("root")
{
    addChild(std::make_shared<HookManager>());
}

std::shared_ptr<HookManager> Root::hooks()
{
    return child<HookManager>("hooks");
}

}
