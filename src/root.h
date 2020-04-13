#ifndef ROOT_H
#define ROOT_H

#include <memory>

#include "child.h"
#include "clientmanager.h"
#include "hookmanager.h"
#include "keymanager.h"
#include "monitormanager.h"
#include "mousemanager.h"
#include "object.h"
#include "rulemanager.h"
#include "settings.h"
#include "tagmanager.h"
#include "theme.h"
#include "tmp.h"

class Ewmh;
class HlwmCommon;
class IpcServer;
class PanelManager;
class RootCommands;
class XConnection;

class Globals {
public:
    Globals() = default;
    int initial_monitors_locked = 0;
    bool exitOnXlibError = false;
    bool importTagsFromEwmh = true;
};

class Root : public Object {
public:
    //static std::shared_ptr<Root> create();
    //static void destroy();
    static std::shared_ptr<Root> get() { return root_; }
    static void setRoot(const std::shared_ptr<Root>& r) { root_ = r; }
    static HlwmCommon common();

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
    std::unique_ptr<PanelManager> panels; // Using "pimpl" to avoid include
    std::unique_ptr<Ewmh> ewmh; // Using "pimpl" to avoid include

private:
    static std::shared_ptr<Root> root_;
};



#endif // ROOT_H
