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
    // initialize non-dependant children (alphabetically)
    hooks.init();
    keys.init();
    root_commands = new RootCommands(this);
    rules.init();
    settings.init(this);
    tags.init(settings());
    theme.init();
    tmp.init();

    // initialize dependant children
    clients.init(*theme(), *settings());
    monitors.init(settings(), tags());

    // provide dependencies
    tags()->setMonitorManager(monitors());

    // set temporary globals
    ::global_tags = tags();
    ::g_monitors = monitors();

    // connect slots
    clients->needsRelayout.connect(monitors(), &MonitorManager::relayoutTag);
}

Root::~Root()
{
    tags()->setMonitorManager({});

    // Note: delete in reverse order of initialization!
    monitors.reset();
    clients.reset();

    tmp.reset();
    theme.reset();
    tags.reset();
    settings.reset();
    rules.reset();
    delete root_commands;
    keys.reset();
    hooks.reset();

    children_.clear(); // avoid possible circular shared_ptr dependency
}

