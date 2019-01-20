#include "root.h"

#include <memory>

#include "clientmanager.h"
#include "decoration.h"
#include "globals.h"
#include "hookmanager.h"
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
    , monitors(*this, "monitors")
    , rules(*this, "rules")
    , settings(*this, "settings")
    , tags(*this, "tags")
    , theme(*this, "theme")
    , tmp(*this, TMP_OBJECT_PATH)
    , globals(g)
{
    // initialize non-dependant children (alphabetically)
    hooks = new HookManager;
    root_commands = new RootCommands(this);
    rules = new RuleManager();
    settings = new Settings(this);
    tags = new TagManager(settings());
    theme = new Theme;
    tmp = new Tmp();

    // initialize dependant children
    clients = new ClientManager(*theme(), *settings());
    monitors = new MonitorManager(settings(), tags());

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
    delete monitors();
    delete clients();

    delete tmp();
    delete theme();
    delete tags();
    delete settings();
    delete rules();
    delete root_commands;
    delete hooks();

    children_.clear(); // avoid possible circular shared_ptr dependency
}

