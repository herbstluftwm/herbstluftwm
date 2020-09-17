#include <X11/X.h>
#include <X11/Xlib.h>
#include <getopt.h>
#include <sys/wait.h>
#include <unistd.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "client.h"
#include "clientmanager.h"
#include "command.h"
#include "ewmh.h"
#include "frametree.h"
#include "globals.h"
#include "hook.h"
#include "ipc-protocol.h"
#include "ipc-server.h"
#include "keymanager.h"
#include "layout.h"
#include "monitordetection.h"
#include "monitormanager.h"
#include "mousemanager.h"
#include "rectangle.h"
#include "root.h"
#include "rootcommands.h"
#include "rulemanager.h"
#include "settings.h"
#include "tagmanager.h"
#include "tmp.h"
#include "utils.h"
#include "xconnection.h"
#include "xmainloop.h"

using std::endl;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::unique_ptr;

// globals:
int g_verbose = 0;
Display*    g_display;
int         g_screen;
Window      g_root;

// module internals:
static char*    g_autostart_path = nullptr; // if not set, then find it in $HOME or $XDG_CONFIG_HOME
static bool     g_exec_before_quit = false;
static char**   g_exec_args = nullptr;
static XMainLoop* g_main_loop = nullptr;

int quit();
int version(Output output);
int print_tag_status_command(int argc, char** argv, Output output);
void execute_autostart_file();
int raise_command(int argc, char** argv, Output output);
int spawn(int argc, char** argv);
int wmexec(int argc, char** argv);
static void remove_zombies(int signal);
int custom_hook_emit(Input input);
int jumpto_command(int argc, char** argv, Output output);

unique_ptr<CommandTable> commands(shared_ptr<Root> root) {
    RootCommands* root_commands = root->root_commands.get();

    ClientManager* clients = root->clients();
    KeyManager *keys = root->keys();
    MonitorManager* monitors = root->monitors();
    MouseManager* mouse = root->mouse();
    RuleManager* rules = root->rules();
    Settings* settings = root->settings();
    TagManager* tags = root->tags();
    Tmp* tmp = root->tmp();

    std::initializer_list<pair<const string,CommandBinding>> init =
    {
        {"quit",           { quit } },
        {"echo",           {root_commands, &RootCommands::echoCommand,
                                           &RootCommands::echoCompletion }},
        {"true",           {[] { return 0; }}},
        {"false",          {[] { return 1; }}},
        {"try",            {root_commands, &RootCommands::tryCommand,
                                           &RootCommands::completeCommandShifted1}},
        {"silent",         {root_commands, &RootCommands::silentCommand,
                                           &RootCommands::completeCommandShifted1}},
        {"reload",         {[] { execute_autostart_file(); return 0; }}},
        {"version",        { version }},
        {"list_commands",  { list_commands }},
        {"list_monitors",  {monitors, &MonitorManager::list_monitors }},
        {"set_monitors",   {monitors, &MonitorManager::setMonitorsCommand,
                                      &MonitorManager::setMonitorsCompletion} },
        {"disjoin_rects",  disjoin_rects_command},
        {"list_keybinds",  {keys, &KeyManager::listKeybindsCommand }},
        {"list_padding",   monitors->byFirstArg(&Monitor::list_padding, &Monitor::noComplete) },
        {"keybind",        {keys, &KeyManager::addKeybindCommand,
                                  &KeyManager::addKeybindCompletion}},
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
        {"bring",          frame_current_bring},
        {"focus_nth",      { tags->frameCommand(&FrameTree::focusNthCommand) }},
        {"cycle",          { tags->frameCommand(&FrameTree::cycleSelectionCommand) }},
        {"cycle_all",      monitors->tagCommand(&HSTag::cycleAllCommand,
                                                &HSTag::cycleAllCompletion)},
        {"cycle_layout",   tags->frameCommand(&FrameTree::cycleLayoutCommand, &FrameTree::cycleLayoutCompletion) },
        {"cycle_frame",    { tags->frameCommand(&FrameTree::cycleFrameCommand) }},
        {"close",          { close_command }},
        {"close_or_remove",{ monitors->tagCommand(&HSTag::closeOrRemoveCommand) }},
        {"close_and_remove",{ monitors->tagCommand(&HSTag::closeAndRemoveCommand) }},
        {"split",          { tags->frameCommand(&FrameTree::splitCommand) }},
        {"resize",         monitors->tagCommand(&HSTag::resizeCommand,
                                                &HSTag::resizeCompletion)},
        {"focus_edge",     frame_focus_edge},
        {"focus",          monitors->tagCommand(&HSTag::focusInDirCommand,
                                                &HSTag::focusInDirCompletion)},
        {"shift_edge",     frame_move_window_edge},
        {"shift",          frame_move_window_command},
        {"shift_to_monitor",shift_to_monitor},
        {"remove",         { tags->frameCommand(&FrameTree::removeFrameCommand) }},
        {"set",            { settings, &Settings::set_cmd,
                                       &Settings::set_complete }},
        {"get",            { settings, &Settings::get_cmd,
                                       &Settings::get_complete }},
        {"toggle",         { settings, &Settings::toggle_cmd,
                                       &Settings::toggle_complete}},
        {"cycle_value",    { settings, &Settings::cycle_value_cmd,
                                       &Settings::cycle_value_complete}},
        {"cycle_monitor",  monitor_cycle_command},
        {"focus_monitor",  monitor_focus_command},
        {"add",            BIND_OBJECT(tags, tag_add_command) },
        {"use",            monitor_set_tag_command},
        {"use_index",      monitor_set_tag_by_index_command},
        {"use_previous",   { monitor_set_previous_tag_command }},
        {"jumpto",         jumpto_command},
        {"floating",       { tags, &TagManager::floatingCmd,
                                   &TagManager::floatingComplete }},
        {"fullscreen",     {clients, &ClientManager::fullscreen_cmd,
                                     &ClientManager::fullscreen_complete}},
        {"pseudotile",     {clients, &ClientManager::pseudotile_cmd,
                                     &ClientManager::pseudotile_complete}},
        {"tag_status",     print_tag_status_command},
        {"merge_tag",      BIND_OBJECT(tags, removeTag)},
        {"rename",         BIND_OBJECT(tags, tag_rename_command) },
        {"move",           BIND_OBJECT(tags, tag_move_window_command) },
        {"rotate",         { tags->frameCommand(&FrameTree::rotateCommand) }},
        {"move_index",     BIND_OBJECT(tags, tag_move_window_by_index_command) },
        {"add_monitor",    BIND_OBJECT(monitors, addMonitor)},
        {"raise_monitor",  { monitors, &MonitorManager::raiseMonitorCommand,
                                       &MonitorManager::raiseMonitorCompletion }},
        {"remove_monitor", BIND_OBJECT(monitors, removeMonitor)},
        {"move_monitor",   monitors->byFirstArg(&Monitor::move_cmd, &Monitor::move_complete) } ,
        {"rename_monitor", monitors->byFirstArg(&Monitor::renameCommand, &Monitor::renameComplete) },
        {"monitor_rect",   monitor_rect_command},
        {"pad",            monitor_set_pad_command},
        {"raise",          raise_command},
        {"rule",           {rules, &RuleManager::addRuleCommand,
                                   &RuleManager::addRuleCompletion}},
        {"unrule",         {rules, &RuleManager::unruleCommand,
                                   &RuleManager::unruleCompletion}},
        {"apply_rules",    {clients, &ClientManager::applyRulesCmd,
                                     &ClientManager::applyRulesCompletion}},
        {"list_rules",     {rules, &RuleManager::listRulesCommand }},
        {"layout",         tags->frameCommand(&FrameTree::dumpLayoutCommand, &FrameTree::dumpLayoutCompletion)},
        {"stack",          { monitors, &MonitorManager::stackCommand }},
        {"dump",           tags->frameCommand(&FrameTree::dumpLayoutCommand, &FrameTree::dumpLayoutCompletion)},
        {"load",           { tags->frameCommand(&FrameTree::loadCommand) }},
        {"complete",       complete_command},
        {"complete_shell", complete_command},
        {"lock",           { [monitors] { monitors->lock(); return 0; } }},
        {"unlock",         { [monitors] { monitors->unlock(); return 0; } }},
        {"lock_tag",       monitors->byFirstArg(&Monitor::lock_tag_cmd, &Monitor::noComplete) },
        {"unlock_tag",     monitors->byFirstArg(&Monitor::unlock_tag_cmd, &Monitor::noComplete) },
        {"set_layout",     { tags->frameCommand(&FrameTree::setLayoutCommand, &FrameTree::setLayoutCompletion) }},
        {"detect_monitors",{ monitors, &MonitorManager::detectMonitorsCommand,
                                       &MonitorManager::detectMonitorsCompletion }},
        {"!",              { root_commands, &RootCommands::negateCommand,
                                            &RootCommands::completeCommandShifted1 }},
        {"chain",          { root_commands, &RootCommands::chainCommand,
                                            &RootCommands::chainCompletion}},
        {"and",            { root_commands, &RootCommands::chainCommand,
                                            &RootCommands::chainCompletion}},
        {"or",             { root_commands, &RootCommands::chainCommand,
                                            &RootCommands::chainCompletion}},
        {"object_tree",    { root_commands, &RootCommands::print_object_tree_command,
                                            &RootCommands::print_object_tree_complete} },
        {"substitute",     { root_commands, &RootCommands::substitute_cmd,
                                            &RootCommands::substitute_complete} },
        {"foreach",        { root_commands, &RootCommands::foreachCmd,
                                            &RootCommands::foreachComplete} },
        {"sprintf",        { root_commands, &RootCommands::sprintf_cmd,
                                            &RootCommands::sprintf_complete} },
        {"new_attr",       { root_commands, &RootCommands::new_attr_cmd,
                                            &RootCommands::new_attr_complete} },
        {"remove_attr",    { root_commands, &RootCommands::remove_attr_cmd,
                                            &RootCommands::remove_attr_complete }},
        {"compare",        { root_commands, &RootCommands::compare_cmd,
                                            &RootCommands::compare_complete} },
        {"getenv",         { root_commands, &RootCommands::getenvCommand,
                                            &RootCommands::getenvUnsetenvCompletion}},
        {"setenv",         { root_commands, &RootCommands::setenvCommand,
                                            &RootCommands::setenvCompletion}},
        {"export",         { root_commands, &RootCommands::exportEnvCommand,
                                            &RootCommands::exportEnvCompletion}},
        {"unsetenv",       { root_commands, &RootCommands::unsetenvCommand,
                                            &RootCommands::getenvUnsetenvCompletion}},
        {"get_attr",       { root_commands, &RootCommands::get_attr_cmd,
                                            &RootCommands::get_attr_complete }},
        {"set_attr",       { root_commands, &RootCommands::set_attr_cmd,
                                            &RootCommands::set_attr_complete }},
        {"attr",           { root_commands, &RootCommands::attr_cmd,
                                            &RootCommands::attr_complete }},
        {"mktemp",         { tmp, &Tmp::mktemp,
                                  &Tmp::mktempComplete }},
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
    output << "Copyright (c) 2011-2020 Thorsten Wißmann" << endl;
    output << "Released under the Simplified BSD License" << endl;
    for (const auto& d : MonitorDetection::detectors()) {
        output << d.name_ << " support: " << (d.detect_ ? "on" : "off") << endl;
    }
    return 0;
}

int print_tag_status_command(int argc, char** argv, Output output) {
    Monitor* monitor;
    if (argc >= 2) {
        monitor = string_to_monitor(argv[1]);
    } else {
        monitor = get_current_monitor();
    }
    if (!monitor) {
        output << argv[0] << ": Monitor \"" << argv[1] << "\" not found!\n";
        return HERBST_INVALID_ARGUMENT;
    }
    tag_update_flags();
    output << '\t';
    for (int i = 0; i < tag_get_count(); i++) {
        HSTag* tag = get_tag_by_index(i);
        // print flags
        char c = '.';
        if (tag->flags & TAG_FLAG_USED) {
            c = ':';
        }
        Monitor *tag_monitor = find_monitor_with_tag(tag);
        if (tag_monitor == monitor) {
            c = '+';
            if (monitor == get_current_monitor()) {
                c = '#';
            }
        } else if (tag_monitor) {
            c = '-';
            if (get_current_monitor() == tag_monitor) {
                c = '%';
            }
        }
        if (tag->flags & TAG_FLAG_URGENT) {
            c = '!';
        }
        output << c;
        output << *tag->name;
        output << '\t';
    }
    return 0;
}

int custom_hook_emit(Input input) {
    hook_emit(input.toVector());
    return 0;
}

static void execvp_helper(char *const command[]) {
    execvp(command[0], command);
    std::cerr << "herbstluftwm: execvp \"" << command << "\"";
    perror(" failed");
}

// spawn() heavily inspired by dwm.c
int spawn(int argc, char** argv) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    if (fork() == 0) {
        // only look in child
        if (g_display) {
            close(ConnectionNumber(g_display));
        }
        // shift all args in argv by 1 to the front
        // so that we have space for a NULL entry at the end for execvp
        char** execargs = argv_duplicate(argc, argv);
        free(execargs[0]);
        int i;
        for (i = 0; i < argc-1; i++) {
            execargs[i] = execargs[i+1];
        }
        execargs[i] = nullptr;
        // do actual exec
        setsid();
        execvp_helper(execargs);
        exit(0);
    }
    return 0;
}

int wmexec(int argc, char** argv) {
    if (argc >= 2) {
        // shift all args in argv by 1 to the front
        // so that we have space for a NULL entry at the end for execvp
        char** execargs = argv_duplicate(argc, argv);
        free(execargs[0]);
        int i;
        for (i = 0; i < argc-1; i++) {
            execargs[i] = execargs[i+1];
        }
        execargs[i] = nullptr;
        // quit and exec to new window manger
        g_exec_args = execargs;
    } else {
        // exec into same command
        g_exec_args = nullptr;
    }
    g_exec_before_quit = true;
    g_main_loop->quit();
    return EXIT_SUCCESS;
}

int raise_command(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto client = get_client(argv[1]);
    if (client) {
        client->raise();
    } else {
        auto window = get_window(argv[1]);
        if (window) {
            XRaiseWindow(g_display, window);
        } else {
            output << argv[0] << ": Could not find client \"" << argv[1] << "\".\n";
            return HERBST_INVALID_ARGUMENT;
        }
    }
    return 0;
}

int jumpto_command(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto client = get_client(argv[1]);
    if (client) {
        focus_client(client, true, true, true);
        return 0;
    } else {
        output << argv[0] << ": Could not find client \"" << argv[1] << "\".\n";
        return HERBST_INVALID_ARGUMENT;
    }
}

void execute_autostart_file() {
    string path;
    if (g_autostart_path) {
        path = g_autostart_path;
    } else {
        // find right directory
        char* xdg_config_home = getenv("XDG_CONFIG_HOME");
        if (xdg_config_home) {
            path = xdg_config_home;
        } else {
            char* home = getenv("HOME");
            if (!home) {
                HSWarning("Will not run autostart file. "
                          "Neither $HOME or $XDG_CONFIG_HOME is set.\n");
                return;
            }
            path = string(home) + "/.config";
        }
        path += "/" HERBSTLUFT_AUTOSTART;
    }
    if (0 == fork()) {
        if (g_display) {
            close(ConnectionNumber(g_display));
        }
        setsid();
        execl(path.c_str(), path.c_str(), nullptr);

        const char* global_autostart = HERBSTLUFT_GLOBAL_AUTOSTART;
        HSDebug("Cannot execute %s, falling back to %s\n", path.c_str(), global_autostart);
        execl(global_autostart, global_autostart, nullptr);

        fprintf(stderr, "herbstluftwm: execvp \"%s\"", global_autostart);
        perror(" failed");
        exit(EXIT_FAILURE);
    }
}

static void parse_arguments(int argc, char** argv, Globals& g) {
    int exit_on_xerror = g.exitOnXlibError;
    int no_tag_import = 0;
    struct option long_options[] = {
        {"version",         0, nullptr, 'v'},
        {"help",            0, nullptr, 'h'},
        {"autostart",       1, nullptr, 'c'},
        {"locked",          0, nullptr, 'l'},
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
                version(std::cout);
                exit(0);
            case 'c':
                g_autostart_path = optarg;
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
    g.exitOnXlibError = exit_on_xerror != 0;
    g.importTagsFromEwmh = (no_tag_import == 0);
}

static void remove_zombies(int) {
    int bgstatus;
    while (waitpid(-1, &bgstatus, WNOHANG) > 0) {
        ;
    }
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
    XConnection::setExitOnError(g.exitOnXlibError);
    XConnection* X = XConnection::connect();
    g_display = X->display();
    if (!g_display) {
        std::cerr << "herbstluftwm: cannot open display" << endl;
        delete X;
        exit(EXIT_FAILURE);
    }
    if (X->checkotherwm()) {
        std::cerr << "herbstluftwm: another window manager is already running" << endl;
        delete X;
        exit(EXIT_FAILURE);
    }
    // remove zombies on SIGCHLD
    sigaction_signal(SIGCHLD, remove_zombies);
    sigaction_signal(SIGINT,  handle_signal);
    sigaction_signal(SIGQUIT, handle_signal);
    sigaction_signal(SIGTERM, handle_signal);
    // set some globals
    g_screen = X->screen();
    g_root = X->root();
    XSelectInput(X->display(), X->root(), SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask|EnterWindowMask|LeaveWindowMask|StructureNotifyMask);

    // setup ipc server
    IpcServer* ipcServer = new IpcServer(*X);

    auto root = make_shared<Root>(g, *X, *ipcServer);
    Root::setRoot(root);
    //test_object_system();

    Commands::initialize(commands(root));

    XMainLoop mainloop(*X, root.get());
    g_main_loop = &mainloop;

    // setup
    if (g.importTagsFromEwmh) {
        const auto& initialState = root->ewmh->initialState();
        for (auto n : initialState.desktopNames) {
            root->tags->add_tag(n.c_str());
        }
    }
    root->monitors()->ensure_monitors_are_available();
    mainloop.scanExistingClients();
    tag_force_update_flags();
    all_monitors_apply_layout();
    root->ewmh->updateAll();
    execute_autostart_file();

    // main loop
    mainloop.run();

    // enforce to clear the root
    root.reset();
    Root::setRoot(root);
    // and then close the x connection
    delete ipcServer;
    delete X;
    // check if we shall restart an other window manager
    if (g_exec_before_quit) {
        if (g_exec_args) {
            // do actual exec
            HSDebug("==> Doing wmexec to %s\n", g_exec_args[0]);
            execvp_helper(g_exec_args);
        }
        // on failure or if no other wm given, then fall back
        HSDebug("==> Doing wmexec to %s\n", argv[0]);
        execvp_helper(argv);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

