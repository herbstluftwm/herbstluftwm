#include "hookmanager.h"

HookManager::HookManager()
    : add_("add"), remove_("remove") {
    wireActions({ &add_, &remove_ });
}

void HookManager::ls(Path path, Output out)
{
    if (path.empty())
        return Object::ls(out);

    auto child = Path::join(path.begin(), path.end());
    if (children_.find(child) != children_.end()) {
        children_[child]->ls({}, out);
    } else {
        out << "child " << child << " not found!" << std::endl; // TODO
    }
}

void HookManager::add(const std::string &path)
{
    //auto h = std::make_shared<NamedHook>(path);
    //h->hook_into(Root::get());
    //addChild(h, "???");
}

void HookManager::remove(const std::string &path)
{
    removeChild(path);
}

void HookManager::trigger(const std::string &action, ArgList args)
{
    if (action == add_.name()) {
        for (auto a : args)
            add(a);
        return;
    }
    if (action == remove_.name()) {
        for (auto a : args)
            remove(a);
        return;
    }
    Object::trigger(action, args);
}

