#include <X11/X.h>
#include <X11/Xlib.h>
#include <getopt.h>
#include <locale.h>
#include <sys/wait.h>
#include <unistd.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#include "autostart.h"
#include "client.h"
#include "clientmanager.h"
#include "command.h"
#include "commandio.h"
#include "css.h"
#include "ewmh.h"
#include "font.h"
#include "fontdata.h"
#include "frametree.h"
#include "globalcommands.h"
#include "globals.h"
#include "hook.h"
#include "ipc-protocol.h"
#include "ipc-server.h"
#include "keymanager.h"
#include "layout.h"
#include "metacommands.h"
#include "monitordetection.h"
#include "monitormanager.h"
#include "mousemanager.h"
#include "rectangle.h"
#include "root.h"
#include "rulemanager.h"
#include "settings.h"
#include "tagmanager.h"
#include "tmp.h"
#include "utils.h"
#include "watchers.h"
#include "xconnection.h"
#include "xmainloop.h"

using std::endl;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

// globals:
int g_verbose = 0;
Display*    g_display;
Window      g_root;

// module internals:
static bool     g_exec_before_quit = false;
static vector<string> g_exec_args = {};
static XMainLoop* g_main_loop = nullptr;

int quit();
int version(Output output);
int spawn(Input input, Output output);
int wmexec(Input input);
int custom_hook_emit(Input input);

unique_ptr<CommandTable> commands(shared_ptr<Root> root) {
    MetaCommands* meta_commands = root->meta_commands.get();
    GlobalCommands* global_cmds = root->global_commands.get();

    Autostart* autostart = root->autostart();
    ClientManager* clients = root->clients();
    KeyManager *keys = root->keys();
    MonitorManager* monitors = root->monitors();
    MouseManager* mouse = root->mouse();
    RuleManager* rules = root->rules();
    Settings* settings = root->settings();
    TagManager* tags = root->tags();
    Tmp* tmp = root->tmp();
    Watchers* watchers = root->watchers();

    std::initializer_list<pair<const string,CommandBinding>> init =
    {
        {"quit",           { quit } },
        {"echo",           {meta_commands, &MetaCommands::echoCommand,
                                           &MetaCommands::echoCompletion }},
        {"true",           {[] { return 0; }}},
        {"false",          {[] { return 1; }}},
        {"try",            {meta_commands, &MetaCommands::tryCommand,
                                           &MetaCommands::completeCommandShifted1}},
        {"silent",         {meta_commands, &MetaCommands::silentCommand,
                                           &MetaCommands::completeCommandShifted1}},
        {"reload",         {autostart, &Autostart::reloadCmdDummyOutput }},
        {"version",        { version }},
        {"list_commands",  { list_commands }},
        {"list_monitors",  {monitors, &MonitorManager::list_monitors }},
        {"set_monitors",   {monitors, &MonitorManager::setMonitorsCommand,
                                      &MonitorManager::setMonitorsCompletion} },
        {"disjoin_rects",  disjoin_rects_command},
        {"list_keybinds",  {keys, &KeyManager::listKeybindsCommand }},
        {"list_padding",   monitors->byFirstArg(&Monitor::list_padding, &Monitor::noComplete) },
        {"keybind",        {keys, &KeyManager::keybindCommand}},
        {"keyunbind",      {keys, &KeyManager::removeKeybindCommand,
                                  &KeyManager::removeKeybindCompletion}},
        {"mousebind",      {mouse, &MouseManager::addMouseBindCommand,
                                   &MouseManager::addMouseBindCompletion}},
        {"mouseunbind",    {mouse, &MouseManager::mouse_unbind_all }},
        {"drag",           {mouse, &MouseManager::dragCommand,
                                   &MouseManager::dragCompletion}},
        {"spawn",          spawn},
        {"wmexec",         wmexec},
        {"emit_hook",      { custom_hook_emit }},
        {"bring",          {global_cmds, &GlobalCommands::bringCommand }},
        {"focus_nth",      {global_cmds, &GlobalCommands::focusNthCommand }},
        {"cycle",          { monitors->tagCommand(&HSTag::cycleCommand) }},
        {"cycle_all",      monitors->tagCommand(&HSTag::cycleAllCommand)},
        {"cycle_layout",   tags->frameCommand(&FrameTree::cycleLayoutCommand, &FrameTree::cycleLayoutCompletion) },
        {"cycle_frame",    { tags->frameCommand(&FrameTree::cycleFrameCommand) }},
        {"close",          { global_cmds, &GlobalCommands::closeCommand }},
        {"close_or_remove",{ monitors->tagCommand(&HSTag::closeOrRemoveCommand) }},
        {"close_and_remove",{ monitors->tagCommand(&HSTag::closeAndRemoveCommand) }},
        {"split",          { tags->frameCommand(&FrameTree::splitCommand) }},
        {"resize",         monitors->tagCommand(&HSTag::resizeCommand,
                                                &HSTag::resizeCompletion)},
        {"focus_edge",     {global_cmds, &GlobalCommands::focusEdgeCommand,
                                         &GlobalCommands::focusEdgeCompletion}},
        {"focus",          monitors->tagCommand(&HSTag::focusInDirCommand)},
        {"shift_edge",     {global_cmds, &GlobalCommands::shiftEdgeCommand,
                                         &GlobalCommands::shiftEdgeCompletion}},
        {"shift",          monitors->tagCommand(&HSTag::shiftInDirCommand)},
        {"shift_to_monitor",{ monitors, &MonitorManager::shiftToMonitorCommand }},
        {"remove",         { tags->frameCommand(&FrameTree::removeFrameCommand) }},
        {"set",            { settings, &Settings::set_cmd,
                                       &Settings::set_complete }},
        {"get",            { settings, &Settings::get_cmd,
                                       &Settings::get_complete }},
        {"toggle",         { settings, &Settings::toggle_cmd,
                                       &Settings::toggle_complete}},
        {"cycle_value",    { global_cmds, &GlobalCommands::cycleValueCommand,
                                          &GlobalCommands::cycleValueCompletion}},
        {"cycle_monitor",  { monitors, &MonitorManager::cycleCommand }},
        {"focus_monitor",  { monitors, &MonitorManager::focusCommand }},
        {"add",            { tags, &TagManager::addCommand }},
        {"use",            { global_cmds, &GlobalCommands::useTagCommand }},
        {"use_index",      { global_cmds, &GlobalCommands::useTagByIndexCommand }},
        {"use_previous",   { global_cmds, &GlobalCommands::usePreviousCommand }},
        {"jumpto",         { global_cmds, &GlobalCommands::jumptoCommand }},
        {"floating",       { tags, &TagManager::floatingCmd,
                                   &TagManager::floatingComplete }},
        {"fullscreen",     {clients, &ClientManager::fullscreen_cmd,
                                     &ClientManager::fullscreen_complete}},
        {"pseudotile",     {clients, &ClientManager::pseudotile_cmd,
                                     &ClientManager::pseudotile_complete}},
        {"tag_status",     {global_cmds, &GlobalCommands::tagStatusCommand}},
        {"merge_tag",      { tags, &TagManager::mergeTagCommand }},
        {"rename",         { tags, &TagManager::tag_rename_command }},
        {"move",           { tags, &TagManager::tag_move_window_command }},
        {"rotate",         { tags->frameCommand(&FrameTree::rotateCommand) }},
        {"mirror",         { tags->frameCommand(&FrameTree::mirrorCommand, &FrameTree::mirrorCompletion) }},
        {"move_index",     { tags, &TagManager::tag_move_window_by_index_command }},
        {"add_monitor",    { monitors, &MonitorManager::addMonitorCommand }},
        {"raise_monitor",  { monitors, &MonitorManager::raiseMonitorCommand,
                                       &MonitorManager::raiseMonitorCompletion }},
        {"remove_monitor", { monitors, &MonitorManager::removeMonitorCommand }},
        {"move_monitor",   monitors->byFirstArg(&Monitor::move_cmd, &Monitor::move_complete) } ,
        {"rename_monitor", monitors->byFirstArg(&Monitor::renameCommand, &Monitor::renameComplete) },
        {"monitor_rect",   { monitors, &MonitorManager::rectCommand }},
        {"pad",            { monitors, &MonitorManager::padCommand }},
        {"raise",          { global_cmds, &GlobalCommands::raiseCommand }},
        {"lower",          { global_cmds, &GlobalCommands::lowerCommand }},
        {"list_clients",   { global_cmds, &GlobalCommands::listClientsCommand }},
        {"rule",           {rules, &RuleManager::addRuleCommand,
                                   &RuleManager::addRuleCompletion}},
        {"unrule",         {rules, &RuleManager::unruleCommand,
                                   &RuleManager::unruleCompletion}},
        {"apply_rules",    {clients, &ClientManager::applyRulesCmd,
                                     &ClientManager::applyRulesCompletion}},
        {"apply_tmp_rule", {clients, &ClientManager::applyTmpRuleCmd,
                                     &ClientManager::applyTmpRuleCompletion}},
        {"list_rules",     {rules, &RuleManager::listRulesCommand }},
        {"layout",         tags->frameCommand(&FrameTree::dumpLayoutCommand, &FrameTree::dumpLayoutCompletion)},
        {"stack",          { monitors, &MonitorManager::stackCommand }},
        {"dump",           tags->frameCommand(&FrameTree::dumpLayoutCommand, &FrameTree::dumpLayoutCompletion)},
        {"load",           { tags->frameCommand(&FrameTree::loadCommand, &FrameTree::loadCompletion ) }},
        {"complete",       completeCommand},
        {"complete_shell", completeCommand},
        {"lock",           { [monitors] { monitors->lock(); return 0; } }},
        {"unlock",         { [monitors] { monitors->unlock(); return 0; } }},
        {"lock_tag",       monitors->byFirstArg(&Monitor::lock_tag_cmd, &Monitor::noComplete) },
        {"unlock_tag",     monitors->byFirstArg(&Monitor::unlock_tag_cmd, &Monitor::noComplete) },
        {"set_layout",     { tags->frameCommand(&FrameTree::setLayoutCommand, &FrameTree::setLayoutCompletion) }},
        {"detect_monitors",{ monitors, &MonitorManager::detectMonitorsCommand,
                                       &MonitorManager::detectMonitorsCompletion }},
        {"!",              { meta_commands, &MetaCommands::negateCommand,
                                            &MetaCommands::completeCommandShifted1 }},
        {"chain",          { meta_commands, &MetaCommands::chainCommand,
                                            &MetaCommands::chainCompletion}},
        {"and",            { meta_commands, &MetaCommands::chainCommand,
                                            &MetaCommands::chainCompletion}},
        {"or",             { meta_commands, &MetaCommands::chainCommand,
                                            &MetaCommands::chainCompletion}},
        {"object_tree",    { meta_commands, &MetaCommands::print_object_tree_command,
                                            &MetaCommands::print_object_tree_complete} },
        {"substitute",     { meta_commands, &MetaCommands::substitute_cmd,
                                            &MetaCommands::substitute_complete} },
        {"foreach",        { meta_commands, &MetaCommands::foreachCommand} },
        {"sprintf",        { meta_commands, &MetaCommands::sprintf_cmd,
                                            &MetaCommands::sprintf_complete} },
        {"new_attr",       { meta_commands, &MetaCommands::new_attr_cmd,
                                            &MetaCommands::new_attr_complete} },
        {"remove_attr",    { meta_commands, &MetaCommands::remove_attr_cmd,
                                            &MetaCommands::remove_attr_complete }},
        {"compare",        { meta_commands, &MetaCommands::compare_cmd,
                                            &MetaCommands::compare_complete} },
        {"getenv",         { meta_commands, &MetaCommands::getenvCommand,
                                            &MetaCommands::getenvUnsetenvCompletion}},
        {"setenv",         { meta_commands, &MetaCommands::setenvCommand,
                                            &MetaCommands::setenvCompletion}},
        {"export",         { meta_commands, &MetaCommands::exportEnvCommand,
                                            &MetaCommands::exportEnvCompletion}},
        {"unsetenv",       { meta_commands, &MetaCommands::unsetenvCommand,
                                            &MetaCommands::getenvUnsetenvCompletion}},
        {"get_attr",       { meta_commands, &MetaCommands::get_attr_cmd,
                                            &MetaCommands::get_attr_complete }},
        {"set_attr",       { meta_commands, &MetaCommands::set_attr_cmd,
                                            &MetaCommands::set_attr_complete }},
        {"attr_type",      { meta_commands, &MetaCommands::attrTypeCommand,
                                            &MetaCommands::attrTypeCompletion }},
        {"help",           { meta_commands, &MetaCommands::helpCommand,
                                            &MetaCommands::helpCompletion }},
        {"attr",           { meta_commands, &MetaCommands::attr_cmd,
                                            &MetaCommands::attr_complete }},
        {"watch",          { watchers, &Watchers::watchCommand,
                                       &Watchers::watchCompletion }},
        {"mktemp",         { tmp, &Tmp::mktemp,
                                  &Tmp::mktempComplete }},
        {"debug_css",      { &debugCssCommand } },
    };
    return unique_ptr<CommandTable>(new CommandTable(init));
}

// core functions
int quit() {
    if (g_main_loop) {
        g_main_loop->quit();
    }
    return 0;
}

int version(Output output) {
    output << WINDOW_MANAGER_NAME << " " << HERBSTLUFT_VERSION << endl;
    output << "Copyright (c) 2011-2021 Thorsten Wißmann" << endl;
    output << "Released under the Simplified BSD License" << endl;
    for (const auto& d : MonitorDetection::detectors()) {
        output << d.name_ << " support: " << (d.detect_ ? "on" : "off") << endl;
    }
    return 0;
}

int custom_hook_emit(Input input) {
    hook_emit(input.toVector());
    return 0;
}

int spawn(Input input, Output output) {
    if (input.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }
    string msg = spawnProcess(input.toVector());
    if (!msg.empty()) {
        output.perror() << msg << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    return 0;
}

int wmexec(Input input) {
    g_exec_args = input.toVector();
    g_exec_before_quit = true;
    g_main_loop->quit();
    return EXIT_SUCCESS;
}

static void parse_arguments(int argc, char** argv, Globals& g) {
    int exit_on_xerror = g.exitOnXlibError;
    int noTransparency = !g.trueTransparency;
    int no_tag_import = 0;
    int replace = 0;
    struct option long_options[] = {
        {"version",         0, nullptr, 'v'},
        {"help",            0, nullptr, 'h'},
        {"autostart",       1, nullptr, 'c'},
        {"locked",          0, nullptr, 'l'},
        {"replace",         0, &replace, 1},
        {"no-transparency", 0, &noTransparency, 1},
        {"exit-on-xerror",  0, &exit_on_xerror, 1},
        {"no-tag-import",   0, &no_tag_import, 1},
        {"verbose",         0, &g_verbose, 1},
        {}
    };
    // parse options
    while (true) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "+c:vlh", long_options, &option_index);
        if (c == -1) {
            break;
        }
        switch (c) {
            case 0:
                /* ignore recognized long option */
                break;
            case 'v':
                {
                    auto stdio = OutputChannels::stdio();
                    version(stdio);
                    exit(0);
                }
            case 'c':
                g.autostartPath = optarg;
                break;
            case 'l':
                g.initial_monitors_locked = 1;
                break;
            case 'h':
                std::cout << "This starts the herbstluftwm window manager. In order to" << endl;
                std::cout << "interact with a running herbstluftwm instance, use the" << endl;
                std::cout << "\'herbstclient\' command." << endl;
                std::cout << endl;
                std::cout << "The herbstluftwm command accepts the following options:" << endl;
                std::cout << endl;
                for (size_t i = 0; long_options[i].name; i++) {
                    std::cout << "    ";
                    if (long_options[i].val != 1) {
                        std::cout << "-" << static_cast<char>(long_options[i].val);
                        if (long_options[i].has_arg) {
                            std::cout << " ARG";
                        }
                        std::cout << ", ";
                    }
                    std::cout << "--" << long_options[i].name;
                    if (long_options[i].has_arg) {
                        std::cout << " ARG";
                    }
                    std::cout << endl;
                }
                std::cout << endl;
                std::cout << "See the herbstluftwm(1) man page for their meaning." << endl;
                exit(EXIT_SUCCESS);
            default:
                exit(EXIT_FAILURE);
        }
    }
    g.replaceExistingWm = replace != 0;
    g.exitOnXlibError = exit_on_xerror != 0;
    g.importTagsFromEwmh = (no_tag_import == 0);
    g.trueTransparency = !noTransparency;
}

static void handle_signal(int signal) {
    HSDebug("Interrupted by signal %d\n", signal);
    if (g_main_loop) {
        g_main_loop->quit();
    }
    return;
}

static void sigaction_signal(int signum, void (*handler)(int)) {
    struct sigaction act = {};
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NOCLDSTOP | SA_RESTART;
    sigaction(signum, &act, nullptr);
}
/* ---- */
/* main */
/* ---- */

int main(int argc, char* argv[]) {
    Globals g;
    parse_arguments(argc, argv, g);
    g.globalAutostartPath = HERBSTLUFT_GLOBAL_AUTOSTART;

    if (!setlocale(LC_CTYPE, "") || !XSupportsLocale()) {
        std::cerr << "warning: no locale support" << endl;
    }
    XConnection::setExitOnError(g.exitOnXlibError);
    XConnection* X = XConnection::connect();
    g_display = X->display();
    if (!g_display) {
        std::cerr << "herbstluftwm: cannot open display" << endl;
        delete X;
        exit(EXIT_FAILURE);
    }
    Ewmh* ewmh = new Ewmh(*X);
    if (!ewmh->acquireScreenSelection(g.replaceExistingWm) || X->otherWmListensRoot()) {
        std::cerr << "herbstluftwm: another window manager is already running (try --replace)" << endl;
        delete X;
        exit(EXIT_FAILURE);
    }
    if (g.trueTransparency) {
        X->tryInitTransparency();
    }
    // ignore sigchld, but set it anyway such that we don't
    // receive anything on stopped children (SA_NOCLDSTOP)
    sigaction_signal(SIGCHLD, [](int) {});
    sigaction_signal(SIGINT,  handle_signal);
    sigaction_signal(SIGQUIT, handle_signal);
    sigaction_signal(SIGTERM, handle_signal);
    // set some globals
    g_root = X->root();
    XSelectInput(X->display(), X->root(), SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask|EnterWindowMask|LeaveWindowMask|StructureNotifyMask);
    ewmh->installWmWindow();

    // setup ipc server
    IpcServer* ipcServer = new IpcServer(*X);
    FontData::s_xconnection = X;
    auto root = make_shared<Root>(g, *X, *ewmh, *ipcServer);
    Root::setRoot(root);
    //test_object_system();

    Commands::initialize(commands(root));

    XMainLoop mainloop(*X, root.get());
    g_main_loop = &mainloop;

    // setup
    if (g.importTagsFromEwmh) {
        const auto& initialState = ewmh->initialState();
        for (auto n : initialState.desktopNames) {
            root->tags->add_tag(n.c_str());
        }
    }
    root->monitors()->ensure_monitors_are_available();
    mainloop.scanExistingClients();
    tag_force_update_flags();
    all_monitors_apply_layout();
    ewmh->updateAll();
    if (!root->clients->focus()) {
        // if no client is focused, focus dummy window:
        ewmh->clearInputFocus();
    }
    mainloop.childExited.connect(root->autostart(), &Autostart::childExited);
    root->autostart()->reloadCmd();

    // main loop
    mainloop.run();

    // Shut everything down. Root::get() still works.
    root->shutdown();
    // clear the root to destroy the object.
    // Now Root::get() does not work anymore.
    root.reset();
    Root::setRoot(root);
    // and then close the x connection
    HSFont::shutdown();
    FontData::s_xconnection = nullptr;
    delete ipcServer;
    delete ewmh;
    delete X;
    // check if we shall restart an other window manager
    if (g_exec_before_quit) {
        if (!g_exec_args.empty()) {
            // do actual exec
            HSDebug("==> Doing wmexec to %s\n", g_exec_args[0].c_str());
            int error = execvp_helper(g_exec_args);
            std::cerr << argv[0] << ": execvp \"" << g_exec_args[0]
                      << "\" failed: " << trimRight(strerror(error), "\n") << endl;
        }
        // on failure or if no other wm given, then fall back
        HSDebug("==> Doing wmexec to %s\n", argv[0]);
        execvp(argv[0], argv);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

