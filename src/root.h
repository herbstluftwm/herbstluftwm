#ifndef ROOT_H
#define ROOT_H

#include "object.h"
#include "child.h"

// new object tree root.

class TagManager;
class HookManager;
class ClientManager;
class MonitorManager;
class Theme;
class Settings;
class Tmp;
class RootCommands;
class RuleManager;

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

    Child_<Settings> settings;
    Child_<ClientManager> clients;
    Child_<TagManager> tags;
    Child_<MonitorManager> monitors;
    Child_<HookManager> hooks;
    Child_<Theme> theme;
    Child_<Tmp> tmp;
    Child_<RuleManager> rules;
    RootCommands* root_commands;
    Globals globals;

private:
    static std::shared_ptr<Root> root_;
};



#endif // ROOT_H
