#include "root.h"

#include <memory>

#include "client.h"
#include "clientmanager.h"
#include "ewmh.h"
#include "hlwmcommon.h"
#include "hookmanager.h"
#include "keymanager.h"
#include "layout.h"
#include "monitormanager.h"
#include "mousemanager.h"
#include "panelmanager.h"
#include "rootcommands.h"
#include "rulemanager.h"
#include "settings.h"
#include "tag.h"
#include "tagmanager.h"
#include "theme.h"
#include "tmp.h"
#include "utils.h"

using std::shared_ptr;

shared_ptr<Root> Root::root_;

Root::Root(Globals g, XConnection& xconnection, IpcServer& ipcServer)
    : clients(*this, "clients")
    , hooks(*this, "hooks")
    , keys(*this, "keys")
    , monitors(*this, "monitors")
    , mouse(*this, "mouse")
    , rules(*this, "rules")
    , settings(*this, "settings")
    , tags(*this, "tags")
    , theme(*this, "theme")
    , tmp(*this, TMP_OBJECT_PATH)
    , globals(g)
    , root_commands(make_unique<RootCommands>(*this))
    , X(xconnection)
    , ipcServer_(ipcServer)
    , panels(make_unique<PanelManager>(xconnection))
    , ewmh(make_unique<Ewmh>(xconnection))
{
    // initialize root children (alphabetically)
    clients.init();
    hooks.init();
    keys.init();
    monitors.init();
    mouse.init();
    rules.init();
    settings.init();
    tags.init();
    theme.init();
    tmp.init();

    // inject dependencies where needed
    ewmh->injectDependencies(this);
    settings->injectDependencies(this);
    tags->injectDependencies(monitors(), settings());
    clients->injectDependencies(settings(), theme(), ewmh.get());
    monitors->injectDependencies(settings(), tags(), panels.get());
    mouse->injectDependencies(clients(), monitors());
    panels->injectDependencies(settings());

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

Root::~Root()
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
    hooks.reset();
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

