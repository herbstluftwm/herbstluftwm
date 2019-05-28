#include "hookmanager.h"

#include "utils.h"

using std::endl;
using std::string;

HookManager::HookManager()
    : add_("add"), remove_("remove") {
    wireActions({ &add_, &remove_ });
}

void HookManager::ls(Path path, Output out)
{
    if (path.empty())
        return Object::ls(out);

    auto child = join_strings(path, ".");
    if (children_.find(child) != children_.end()) {
        children_[child]->ls({}, out);
    } else {
        out << "child " << child << " not found!" << endl; // TODO
    }
}

void HookManager::add(const string &path)
{
    //auto h = make_shared<NamedHook>(path);
    //h->hook_into(Root::get());
    //addChild(h, "???");
}

void HookManager::remove(const string &path)
{
    removeChild(path);
}
