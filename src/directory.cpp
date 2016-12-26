#include "directory.h"
#include "hook.h"

#include <iostream>
#include <string>

namespace herbstluft {

using namespace std;

void Directory::notifyHooks(HookEvent event, const std::string& arg)
{
    for (auto hook : hooks_) {
        auto h = hook.second.lock();
        if (h) {
            (*h)(shared_from_this(), event, arg);
        } // TODO: else throw
    }
}

void Directory::addChild(std::shared_ptr<Directory> child, std::string name)
{
    if (name == "") {
        name = child->name();
    }
    children_[name] = child;
    notifyHooks(HookEvent::CHILD_ADDED, name);
}

static void null_deleter(Directory *) {}

void Directory::addStaticChild(Directory *child, std::string name)
{
    if (name == "") {
        name = child->name();
    }
    children_[name] = shared_ptr<Directory>(child, null_deleter);
    notifyHooks(HookEvent::CHILD_ADDED, name);
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
    out << children_.size() << (children_.size() == 1 ? " child" : " children")
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

class DirectoryTreeInterface : public TreeInterface {
public:
    DirectoryTreeInterface(string label, Ptr(Directory) d) : lbl(label), dir(d) {
        for (auto child : dir->children()) {
            buf.push_back(child);
        }
    };
    size_t childCount() {
        return buf.size();
    };
    Ptr(TreeInterface) nthChild(size_t idx) {
        return make_shared<DirectoryTreeInterface>(buf[idx].first, buf[idx].second);
    };
    void appendCaption(Output output) {
        output << lbl;
    };
private:
    string lbl;
    vector<pair<string,Ptr(Directory)>> buf;
    Ptr(Directory) dir;
};

void Directory::printTree(Output output, std::string rootLabel) {
    Ptr(TreeInterface) intface = make_shared<DirectoryTreeInterface>(rootLabel, ptr<Directory>());
    tree_print_to(intface, output);
}

}
