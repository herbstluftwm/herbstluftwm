#include "root.h"
#include "tagmanager.h"
#include "hookmanager.h"
#include "clientmanager.h"
#include "monitormanager.h"
#include "monitor.h"
#include "attribute.h"
#include "ipc-protocol.h"
#include "globals.h"
#include "settings.h"
#include "tmp.h"
#include "decoration.h"
#include "client.h"
#include "rootcommands.h"

#include <memory>
#include <stdexcept>
#include <sstream>

std::shared_ptr<Root> Root::root_;

Root::Root(Globals g)
    : settings(*this, "settings")
    , clients(*this, "clients")
    , tags(*this, "tags")
    , monitors(*this, "monitors")
    , hooks(*this, "hooks")
    , theme(*this, "theme")
    , tmp(*this, TMP_OBJECT_PATH)
    , globals(g)
{
    settings = new Settings(this);
    theme = new Theme;
    clients = new ClientManager(*theme(), *settings());
    tags = new TagManager(settings());
    monitors = new MonitorManager(settings(), tags());
    tags()->setMonitorManager(monitors());
    hooks = new HookManager;
    tmp = new Tmp(this);
    root_commands = new RootCommands(this);

    // set temporary globals
    ::tags = tags();
    ::g_monitors = monitors();
}

Root::~Root()
{
    tags()->setMonitorManager({});
    // Note: delete in the right order!
    delete root_commands;
    delete tmp();
    delete hooks();
    delete monitors();
    delete tags();
    delete clients();
    delete theme();
    delete settings();
    children_.clear(); // avoid possible circular shared_ptr dependency
}

