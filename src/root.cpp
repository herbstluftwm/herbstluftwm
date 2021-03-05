#include "root.h"

#include <memory>

#include "client.h"
#include "clientmanager.h"
#include "ewmh.h"
#include "globalcommands.h"
#include "hlwmcommon.h"
#include "keymanager.h"
#include "layout.h"
#include "metacommands.h"
#include "monitormanager.h"
#include "mousemanager.h"
#include "panelmanager.h"
#include "rulemanager.h"
#include "settings.h"
#include "tag.h"
#include "tagmanager.h"
#include "theme.h"
#include "tmp.h"
#include "utils.h"
#include "watchers.h"

using std::shared_ptr;

shared_ptr<Root> Root::root_;

Root::Root(Globals g, XConnection& xconnection, Ewmh& ewmh, IpcServer& ipcServer)
    : clients(*this, "clients")
    , keys(*this, "keys")
    , monitors(*this, "monitors")
    , mouse(*this, "mouse")
    , rules(*this, "rules")
    , settings(*this, "settings")
    , tags(*this, "tags")
    , theme(*this, "theme")
    , tmp(*this, TMP_OBJECT_PATH)
    , watchers(*this, "watchers")
    , globals(g)
    , meta_commands(make_unique<MetaCommands>(*this))
    , global_commands(make_unique<GlobalCommands>(*this))
    , X(xconnection)
    , ipcServer_(ipcServer)
    , panels(make_unique<PanelManager>(xconnection))
    , ewmh_(ewmh)
{
    // initialize root children (alphabetically)
    clients.init();
    keys.init();
    monitors.init();
    mouse.init();
    rules.init();
    settings.init();
    tags.init();
    theme.init();
    tmp.init();
    watchers.init();

    // inject dependencies where needed
    ewmh_.injectDependencies(this);
    settings->injectDependencies(this);
    tags->injectDependencies(monitors(), settings());
    clients->injectDependencies(settings(), theme(), &ewmh_);
    monitors->injectDependencies(settings(), tags(), panels.get());
    mouse->injectDependencies(clients(), monitors());
    panels->injectDependencies(settings());
    watchers->injectDependencies(this);

    // set temporary globals
    ::global_tags = tags();
    ::g_monitors = monitors();

    // connect slots
    clients->needsRelayout.connect(monitors(), &MonitorManager::relayoutTag);
    tags->needsRelayout_.connect(monitors(), &MonitorManager::relayoutTag);
    clients->clientStateChanged.connect([](Client* c) {
        c->tag()->applyClientState(c);
    });
    theme->theme_changed_.connect(monitors(), &MonitorManager::relayoutAll);
    panels->panels_changed_.connect(monitors(), &MonitorManager::autoUpdatePads);
}

Root::~Root() {
}

void Root::shutdown()
{
    // Note: delete in reverse order of initialization!
    mouse.reset();
    // ClientManager and MonitorManager have circular dependencies, but only
    // MonitorManager needs the other for shutting down, so we do that first:
    monitors.reset();

    // Shut down children with dependencies first:
    clients.reset();
    tags.reset();

    // For the rest, order does not matter (do it alphabetically):
    keys.reset();
    rules.reset();
    settings.reset();
    theme.reset();
    tmp.reset();

    children_.clear(); // avoid possible circular shared_ptr dependency
}

void Root::focusFrame(shared_ptr<FrameLeaf> frameToFocus)
{
    Monitor* monitor = monitors->byFrame(frameToFocus);
    if (!monitor) {
        return;
    }
    monitors->lock();
    monitor->tag->focusFrame(frameToFocus);
    monitor_focus_by_index(static_cast<unsigned int>(monitor->index()));
    monitors->unlock();
}

HlwmCommon Root::common() {
    return HlwmCommon(Root::get().get());
}

