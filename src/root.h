#ifndef ROOT_H
#define ROOT_H

#include <memory>

#include "child.h"
#include "object.h"

// new object tree root.

class ClientManager; // IWYU pragma: keep
class Ewmh;
class FrameLeaf;
class GlobalCommands;
class HlwmCommon;
class IpcServer;
class KeyManager; // IWYU pragma: keep
class MonitorManager; // IWYU pragma: keep
class MouseManager; // IWYU pragma: keep
class PanelManager;
class MetaCommands;
class RuleManager; // IWYU pragma: keep
class Settings; // IWYU pragma: keep
class TagManager; // IWYU pragma: keep
class Theme; // IWYU pragma: keep
class Tmp; // IWYU pragma: keep
class TypesDoc; // IWYU pragma: keep
class Watchers;
class XConnection;

class Globals {
public:
    Globals() = default;
    int initial_monitors_locked = 0;
    bool replaceExistingWm = false;
    bool exitOnXlibError = false;
    bool importTagsFromEwmh = true;
    bool trueTransparency = true; // try true transparency via xrender
};

class Root : public Object {
public:
    //static std::shared_ptr<Root> create();
    //static void destroy();
    static std::shared_ptr<Root> get() { return root_; }
    static void setRoot(const std::shared_ptr<Root>& r) { root_ = r; }
    static HlwmCommon common();

    // constructor creates top-level objects
    Root(Globals g, XConnection& xconnection, Ewmh& ewmh, IpcServer& ipcServer);
    ~Root() override;
    void shutdown();

    // (in alphabetical order)
    Child_<ClientManager> clients;
    Child_<KeyManager> keys;
    Child_<MonitorManager> monitors;
    Child_<MouseManager> mouse;
    Child_<PanelManager> panels;
    Child_<RuleManager> rules;
    Child_<Settings> settings;
    Child_<TagManager> tags;
    Child_<Theme> theme;
    Child_<Tmp> tmp;
    Child_<TypesDoc> types;
    Child_<Watchers> watchers;

    Globals globals;
    std::unique_ptr<MetaCommands> meta_commands; // Using "pimpl" to avoid include
    std::unique_ptr<GlobalCommands> global_commands; // Using "pimpl" to avoid include
    XConnection& X;
    IpcServer& ipcServer_;
    //! Temporary member. In the long run, ewmh should get its information
    // automatically from the signals emitted by ClientManager, etc
    Ewmh& ewmh_;

    // global actions
    void focusFrame(std::shared_ptr<FrameLeaf> frameToFocus);

private:
    static std::shared_ptr<Root> root_;
};



#endif // ROOT_H
