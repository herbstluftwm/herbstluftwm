#include "root.h"

#include <memory>

#include "clientmanager.h"
#include "decoration.h"
#include "globals.h"
#include "hookmanager.h"
#include "keymanager.h"
#include "monitormanager.h"
#include "rootcommands.h"
#include "rulemanager.h"
#include "settings.h"
#include "tagmanager.h"
#include "tmp.h"
#include "utils.h"

std::shared_ptr<Root> Root::root_;

Root::Root(Globals g)
    : clients(*this, "clients")
    , hooks(*this, "hooks")
    , keys(*this, "keys")
    , monitors(*this, "monitors")
    , rules(*this, "rules")
    , settings(*this, "settings")
    , tags(*this, "tags")
    , theme(*this, "theme")
    , tmp(*this, TMP_OBJECT_PATH)
    , globals(g)
{
    // initialize root children (alphabetically)
    clients.init();
    hooks.init();
    keys.init();
    monitors.init();
    root_commands = new RootCommands(this);
    rules.init();
    settings.init();
    tags.init();
    theme.init();
    tmp.init();

    // inject dependencies where needed
    settings->injectDependencies(this);
    tags->injectDependencies(monitors(), settings());
    clients->injectDependencies(settings(), theme());
    monitors->injectDependencies(settings(), tags());

    // set temporary globals
    ::global_tags = tags();
    ::g_monitors = monitors();

    // connect slots
    clients->needsRelayout.connect(monitors(), &MonitorManager::relayoutTag);
}

Root::~Root()
{
    // ClientManager and MonitorManager have circular dependencies, but only
    // MonitorManager needs the other for shutting down, so we do that first:
    monitors.reset();

    // Shut down children with dependencies first:
    clients.reset();
    tags.reset();

    // For the rest, order does not matter (do it alphabetically):
    delete root_commands;
    hooks.reset();
    keys.reset();
    rules.reset();
    settings.reset();
    theme.reset();
    tmp.reset();

    children_.clear(); // avoid possible circular shared_ptr dependency
}

