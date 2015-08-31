#include "root.h"
#include "hookmanager.h"
#include "clientmanager.h"

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
    addChild(std::make_shared<ClientManager>());
}

std::shared_ptr<HookManager> Root::hooks()
{
    return root_->child<HookManager>("hooks");
}

std::shared_ptr<ClientManager> Root::clients() {
    return root_->child<ClientManager>("clients");
}

void Root::cmd_ls(Input in, Output out)
{
    if (in.empty())
        return root_->ls(out);

    Path p(in.front());
    root_->ls(p, out);
}

int print_object_tree_command(int argc, char* argv[], Output output) {
    Root::get()->printTree(output);
}


}
