#ifndef ROOT_H
#define ROOT_H

#include "child.h"
#include "object.h"

#include <memory>

// new object tree root.

class ClientManager;
class HookManager;
class KeyManager;
class MonitorManager;
class RootCommands;
class RuleManager;
class Settings;
class TagManager;
class Theme;
class Tmp;

class Globals {
public:
    Globals() = default;
    int initial_monitors_locked = 0;
};

class Root : public Object {
public:
    //static std::shared_ptr<Root> create();
    //static void destroy();
    static std::shared_ptr<Root> get() { return root_; }
    static void setRoot(const std::shared_ptr<Root>& r) { root_ = r; }

    // constructor creates top-level objects
    Root(Globals g);
    ~Root() override;

    // (in alphabetical order)
    Child_<ClientManager> clients;
    Child_<HookManager> hooks;
    Child_<KeyManager> keys;
    Child_<MonitorManager> monitors;
    Child_<RuleManager> rules;
    Child_<Settings> settings;
    Child_<TagManager> tags;
    Child_<Theme> theme;
    Child_<Tmp> tmp;

    std::unique_ptr<RootCommands> root_commands;
    Globals globals;

private:
    static std::shared_ptr<Root> root_;
};



#endif // ROOT_H
