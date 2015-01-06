#include "hookmanager.h"
#include "hook.h"
#include "root.h"

namespace herbstluft {

HookManager::HookManager()
    : Object("hooks"),
      add_("add"), remove_("remove") {
    wireActions({ &add_, &remove_ });
}

void HookManager::add(const std::string &path)
{
    auto h = std::make_shared<Hook>(path);
    h->hook_into(Root::get());
    addChild(h);
}

void HookManager::remove(const std::string &path)
{
    removeChild(path);
}

void HookManager::trigger(const std::string &action, const Arg &args)
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

}
