#include "directory.h"
#include "hook.h"

#include <iostream>

namespace herbstluft {

void Directory::notifyHooks(const std::string& arg)
{
    std::shared_ptr<Directory> self = self_.lock(); // always works
    for (auto hook : hooks_) {
        auto h = hook.second.lock();
        if (h) {
            (*h)(self, arg);
        } // TODO: else throw
    }
}

void Directory::addChild(std::shared_ptr<Directory> child)
{
    children_[child->name()] = child;
    notifyHooks();
}

void Directory::removeChild(const std::string &child)
{
    children_.erase(child);
    notifyHooks();
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

}
