#include "directory.h"
#include "hook.h"

#include <iostream>

namespace herbstluft {

void Directory::notifyHooks(HookEvent event, const std::string& arg)
{
    for (auto hook : hooks_) {
        auto h = hook.second.lock();
        if (h) {
            (*h)(shared_from_this(), event, arg);
        } // TODO: else throw
    }
}

void Directory::addChild(std::shared_ptr<Directory> child)
{
    children_[child->name()] = child;
    notifyHooks(HookEvent::CHILD_ADDED, child->name());
}

void Directory::removeChild(const std::string &child)
{
    children_.erase(child);
    notifyHooks(HookEvent::CHILD_REMOVED, child);
}

void Directory::addHook(std::shared_ptr<Hook> hook)
{
    hooks_[hook->name()] = hook;
}

void Directory::removeHook(const std::string &hook)
{
    hooks_.erase(hook);
}

void Directory::print(const std::string &prefix)
{
    std::cout << prefix << "==== " << typestr() << " " << name_ << ":" << std::endl;
    if (!children_.empty()) {
        std::cout << prefix << "Children:" << std::endl;
        for (auto it : children_) {
            it.second->print(prefix + "\t| ");
        }
        std::cout << prefix << std::endl;
    }
    if (!hooks_.empty()) {
        std::cout << prefix << "Current hooks:" << std::endl;
        for (auto it : hooks_) {
            std::cout << prefix << "\t" << it.first << std::endl;
        }
    }
}

void Directory::ls(Output out)
{
    out << children_.size() << " children"
        << (children_.size() > 0 ? ":" : ".") << std::endl;
    for (auto it : children_) {
        out << "  " << it.first << "." << std::endl;
    }
}

void Directory::ls(Path path, Output out)
{
    if (path.empty())
        return ls(out);

    auto child = path.front();
    if (exists(child)) {
        children_[child]->ls(path + 1, out);
    } else {
        out << name_ << ": " << child << " not found!" << std::endl; // TODO
    }
}

}
