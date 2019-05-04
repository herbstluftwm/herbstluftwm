#ifndef ROOT_H
#define ROOT_H

#include <memory>

#include "child.h"
#include "object.h"

// new object tree root.

class ClientManager;
class Ewmh;
class HookManager;
class IpcServer;
class KeyManager;
class MonitorManager;
class MouseManager;
class RootCommands;
class RuleManager;
class Settings;
class TagManager;
class Theme;
class Tmp;
class XConnection;

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
    Root(Globals g, XConnection& xconnection, IpcServer& ipcServer);
    ~Root() override;

    // (in alphabetical order)
    Child_<ClientManager> clients;
    Child_<HookManager> hooks;
    Child_<KeyManager> keys;
    Child_<MonitorManager> monitors;
    Child_<MouseManager> mouse;
    Child_<RuleManager> rules;
    Child_<Settings> settings;
    Child_<TagManager> tags;
    Child_<Theme> theme;
    Child_<Tmp> tmp;

    Globals globals;
    std::unique_ptr<RootCommands> root_commands; // Using "pimpl" to avoid include
    XConnection& X;
    IpcServer& ipcServer_;
    //! Temporary member. In the long run, ewmh should get its information
    // automatically from the signals emitted by ClientManager, etc
    std::unique_ptr<Ewmh> ewmh; // Using "pimpl" to avoid include

private:
    static std::shared_ptr<Root> root_;
};



#endif // ROOT_H
