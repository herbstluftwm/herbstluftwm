#ifndef ROOT_H
#define ROOT_H

#include "utils.h"
#include "object.h"
#include "child.h"

// new object tree root.

class Attribute;
class TagManager;
class HookManager;
class ClientManager;
class MonitorManager;
class Theme;
class Settings;
class Tmp;
class RootCommands;

class Globals {
public:
    Globals() : initial_monitors_locked(0) {};
    int initial_monitors_locked;
};

class Root : public Object {
public:
    //static std::shared_ptr<Root> create();
    //static void destroy();
    static std::shared_ptr<Root> get() { return root_; }
    static void setRoot(const std::shared_ptr<Root>& r) { root_ = r; }

    // constructor creates top-level objects
    Root(Globals g);
    ~Root();

    Child_<Settings> settings;
    Child_<ClientManager> clients;
    Child_<TagManager> tags;
    Child_<MonitorManager> monitors;
    Child_<HookManager> hooks;
    Child_<Theme> theme;
    Child_<Tmp> tmp;
    RootCommands* root_commands;
    Globals globals;

private:
    static std::shared_ptr<Root> root_;
};



#endif // ROOT_H
