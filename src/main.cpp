#include <errno.h>
#include <getopt.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "client.h"
#include "clientmanager.h"
#include "command.h"
#include "ewmh.h"
#include "globals.h"
#include "hook.h"
#include "ipc-protocol.h"
#include "ipc-server.h"
#include "key.h"
#include "keymanager.h"
#include "layout.h"
#include "monitormanager.h"
#include "mouse.h"
#include "rectangle.h"
#include "root.h"
#include "rootcommands.h"
#include "rulemanager.h"
#include "settings.h"
#include "stack.h"
#include "tagmanager.h"
#include "tmp.h"
#include "utils.h"
#include "xconnection.h"

using std::unique_ptr;

// globals:
int g_verbose = 0;
Display*    g_display;
int         g_screen;
Window      g_root;
int         g_screen_width;
int         g_screen_height;
bool        g_aboutToQuit;

// module internals:
static char*    g_autostart_path = nullptr; // if not set, then find it in $HOME or $XDG_CONFIG_HOME
static bool     g_exec_before_quit = false;
static char**   g_exec_args = nullptr;

typedef void (*HandlerTable[LASTEvent]) (Root*, XEvent*);

int quit();
int version(Output output);
int echo(int argc, char* argv[], Output output);
int true_command();
int false_command();
int try_command(int argc, char* argv[], Output output);
int silent_command(int argc, char* argv[]);
int print_layout_command(int argc, char** argv, Output output);
int load_command(int argc, char** argv, Output output);
int print_tag_status_command(int argc, char** argv, Output output);
void execute_autostart_file();
int raise_command(int argc, char** argv, Output output);
int spawn(int argc, char** argv);
int wmexec(int argc, char** argv);
static void remove_zombies(int signal);
int custom_hook_emit(int argc, const char** argv);
int jumpto_command(int argc, char** argv, Output output);
int getenv_command(int argc, char** argv, Output output);
int setenv_command(int argc, char** argv, Output output);
int unsetenv_command(int argc, char** argv, Output output);

// handler for X-Events
void buttonpress(Root* root, XEvent* event);
void buttonrelease(Root* root, XEvent* event);
void createnotify(Root* root, XEvent* event);
void configurerequest(Root* root, XEvent* event);
void configurenotify(Root* root, XEvent* event);
void destroynotify(Root* root, XEvent* event);
void enternotify(Root* root, XEvent* event);
void expose(Root* root, XEvent* event);
void focusin(Root* root, XEvent* event);
void keypress(Root* root, XEvent* event);
void mappingnotify(Root* root, XEvent* event);
void motionnotify(Root* root, XEvent* event);
void mapnotify(Root* root, XEvent* event);
void maprequest(Root* root, XEvent* event);
void propertynotify(Root* root, XEvent* event);
void unmapnotify(Root* root, XEvent* event);

unique_ptr<CommandTable> commands(std::shared_ptr<Root> root) {
    RootCommands* root_commands = root->root_commands;

    ClientManager* clients = root->clients();
    KeyManager *keys = root->keys();
    MonitorManager* monitors = root->monitors();
    RuleManager* rules = root->rules();
    Settings* settings = root->settings();
    TagManager* tags = root->tags();
    Tmp* tmp = root->tmp();

    std::initializer_list<std::pair<const std::string,CommandBinding>> init =
    {
        {"quit",           { quit } },
        {"echo",           echo},
        {"true",           {[] { return 0; }}},
        {"false",          {[] { return 1; }}},
        {"try",            try_command},
        {"silent",         silent_command},
        {"reload",         {[] { execute_autostart_file(); return 0; }}},
        {"version",        { version }},
        {"list_commands",  { list_commands }},
        {"list_monitors",  {[monitors] (Output o) { return monitors->list_monitors(o);}}},
        {"set_monitors",   set_monitor_rects_command},
        {"disjoin_rects",  disjoin_rects_command},
        {"list_keybinds",  { [keys] (Output o) { return keys->listKeybindsCommand(o); } }},
        {"list_padding",   monitors->byFirstArg(&Monitor::list_padding) },
        {"keybind",        { [keys] (Input i, Output o) { return keys->addKeybindCommand(i, o); } }},
        {"keyunbind",      {keys, &KeyManager::removeKeybindCommand,
                                  &KeyManager::removeKeybindCompletion}},
        {"mousebind",      mouse_bind_command},
        {"mouseunbind",    { mouse_unbind_all }},
        {"spawn",          spawn},
        {"wmexec",         wmexec},
        {"emit_hook",      custom_hook_emit},
        {"bring",          frame_current_bring},
        {"focus_nth",      frame_current_set_selection},
        {"cycle",          frame_current_cycle_selection},
        {"cycle_all",      cycle_all_command},
        {"cycle_layout",   frame_current_cycle_client_layout},
        {"cycle_frame",    cycle_frame_command},
        {"close",          { close_command }},
        {"close_or_remove",{ close_or_remove_command }},
        {"close_and_remove",{close_and_remove_command }},
        {"split",          frame_split_command},
        {"resize",         frame_change_fraction_command},
        {"focus_edge",     frame_focus_edge},
        {"focus",          frame_focus_command},
        {"shift_edge",     frame_move_window_edge},
        {"shift",          frame_move_window_command},
        {"shift_to_monitor",shift_to_monitor},
        {"remove",         { frame_remove_command }},
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
        {"floating",       tag_set_floating_command},
        {"fullscreen",     {clients, &ClientManager::fullscreen_cmd,
                                     &ClientManager::fullscreen_complete}},
        {"pseudotile",     {clients, &ClientManager::pseudotile_cmd,
                                     &ClientManager::pseudotile_complete}},
        {"tag_status",     print_tag_status_command},
        {"merge_tag",      BIND_OBJECT(tags, removeTag)},
        {"rename",         BIND_OBJECT(tags, tag_rename_command) },
        {"move",           BIND_OBJECT(tags, tag_move_window_command) },
        {"rotate",         { layout_rotate_command }},
        {"move_index",     BIND_OBJECT(tags, tag_move_window_by_index_command) },
        {"add_monitor",    BIND_OBJECT(monitors, addMonitor)},
        {"raise_monitor",  monitor_raise_command},
        {"remove_monitor", BIND_OBJECT(monitors, removeMonitor)},
        {"move_monitor",   monitors->byFirstArg(&Monitor::move_cmd) } ,
        {"rename_monitor", rename_monitor_command},
        {"monitor_rect",   monitor_rect_command},
        {"pad",            monitor_set_pad_command},
        {"raise",          raise_command},
        {"rule",           {rules, &RuleManager::addRuleCommand,
                                   &RuleManager::addRuleCompletion}},
        {"unrule",         {rules, &RuleManager::unruleCommand,
                                   &RuleManager::unruleCompletion}},
        {"list_rules",     {[rules] (Output o) { return rules->listRulesCommand(o); }}},
        {"layout",         print_layout_command},
        {"stack",          print_stack_command},
        {"dump",           print_layout_command},
        {"load",           load_command},
        {"complete",       complete_command},
        {"complete_shell", complete_command},
        {"lock",           { [monitors] { monitors->lock(); return 0; } }},
        {"unlock",         { [monitors] { monitors->unlock(); return 0; } }},
        {"lock_tag",       monitors->byFirstArg(&Monitor::lock_tag_cmd) },
        {"unlock_tag",     monitors->byFirstArg(&Monitor::unlock_tag_cmd) },
        {"set_layout",     frame_current_set_client_layout},
        {"detect_monitors",detect_monitors_command},
        {"chain",          command_chain_command},
        {"and",            command_chain_command},
        {"or",             command_chain_command},
        {"!",              negate_command},
        {"object_tree",    { root_commands, &RootCommands::print_object_tree_command,
                                            &RootCommands::print_object_tree_complete} },
        {"substitute",     BIND_OBJECT(root_commands, substitute_cmd) },
        {"sprintf",        BIND_OBJECT(root_commands, sprintf_cmd) },
        {"new_attr",       BIND_OBJECT(root_commands, new_attr_cmd) },
        {"remove_attr",    { root_commands, &RootCommands::remove_attr_cmd,
                                            &RootCommands::remove_attr_complete }},
        {"compare",        BIND_OBJECT(root_commands, compare_cmd) },
        {"getenv",         getenv_command},
        {"setenv",         setenv_command},
        {"unsetenv",       unsetenv_command},
        {"get_attr",       { root_commands, &RootCommands::get_attr_cmd,
                                            &RootCommands::get_attr_complete }},
        {"set_attr",       { root_commands, &RootCommands::set_attr_cmd,
                                            &RootCommands::set_attr_complete }},
        {"attr",           { root_commands, &RootCommands::attr_cmd,
                                            &RootCommands::attr_complete }},
        {"mktemp",         BIND_OBJECT(tmp, mktemp) },
    };
    return unique_ptr<CommandTable>(new CommandTable(init));
}

// core functions
int quit() {
    g_aboutToQuit = true;
    return 0;
}

int version(Output output) {
    output << WINDOW_MANAGER_NAME << " " << HERBSTLUFT_VERSION << std::endl;
    output << "Copyright (c) 2011-2014 Thorsten Wißmann" << std::endl;
    output << "Released under the Simplified BSD License" << std::endl;
    return 0;
}

int echo(int argc, char* argv[], Output output) {
    if (SHIFT(argc, argv)) {
        // if there still is an argument
        output << argv[0];
        while (SHIFT(argc, argv)) {
            output << " " << argv[0];
        }
    }
    output << '\n';
    return 0;
}

int try_command(int argc, char* argv[], Output output) {
    if (argc <= 1) {
        return HERBST_NEED_MORE_ARGS;
    }
    (void)SHIFT(argc, argv);
    call_command(argc, argv, output);
    return 0;
}

int silent_command(int argc, char* argv[]) {
    if (argc <= 1) {
        return HERBST_NEED_MORE_ARGS;
    }
    (void)SHIFT(argc, argv);
    return call_command_no_output(argc, argv);
}

// prints or dumps the layout of an given tag
// first argument tells whether to print or to dump
int print_layout_command(int argc, char** argv, Output output) {
    HSTag* tag = nullptr;
    // an empty argv[1] means current focused tag
    if (argc >= 2 && argv[1][0] != '\0') {
        tag = find_tag(argv[1]);
        if (!tag) {
            output << argv[0] << ": Tag \"" << argv[1] << "\" not found\n";
            return HERBST_INVALID_ARGUMENT;
        }
    } else { // use current tag
        Monitor* m = get_current_monitor();
        tag = m->tag;
    }
    assert(tag);

    std::shared_ptr<HSFrame> frame = tag->frame->lookup(argc >= 3 ? argv[2] : "");
    if (argc > 0 && !strcmp(argv[0], "dump")) {
        frame->dump(output);
    } else {
        print_frame_tree(frame, output);
    }
    return 0;
}

int load_command(int argc, char** argv, Output output) {
    // usage: load TAG LAYOUT
    HSTag* tag = nullptr;
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    char* layout_string = argv[1];
    if (argc >= 3) {
        tag = find_tag(argv[1]);
        layout_string = argv[2];
        if (!tag) {
            output << argv[0] << ": Tag \"" << argv[1] << "\" not found\n";
            return HERBST_INVALID_ARGUMENT;
        }
    } else { // use current tag
        Monitor* m = get_current_monitor();
        tag = m->tag;
    }
    assert(tag != nullptr);
    char* rest = load_frame_tree(tag->frame, layout_string, output);
    tag_set_flags_dirty(); // we probably changed some window positions
    // arrange monitor
    Monitor* m = find_monitor_with_tag(tag);
    if (m) {
        tag->frame->setVisibleRecursive(true);
        if (get_current_monitor() == m) {
            frame_focus_recursive(tag->frame);
        }
        m->applyLayout();
    } else {
        tag->frame->setVisibleRecursive(false);
    }
    if (!rest) {
        output << argv[0] << ": Error while parsing!\n";
        return HERBST_INVALID_ARGUMENT;
    }
    if (rest[0] != '\0') { // if string was not parsed completely
        output << argv[0] << ": Layout description was too long\n";
        output << argv[0] << ": \"" << rest << "\" has not been parsed\n";
        return HERBST_INVALID_ARGUMENT;
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

int custom_hook_emit(int argc, const char** argv) {
    hook_emit(argc - 1, argv + 1);
    return 0;
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
        execvp(execargs[0], execargs);
        fprintf(stderr, "herbstluftwm: execvp \"%s\"", argv[1]);
        perror(" failed");
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
    g_aboutToQuit = true;
    return EXIT_SUCCESS;
}

int raise_command(int argc, char** argv, Output) {
    auto client = get_client((argc > 1) ? argv[1] : "");
    if (client) {
        client->raise();
    } else {
        auto window = get_window((argc > 1) ? argv[1] : "");
        if (window)
            XRaiseWindow(g_display, std::stoul(argv[1], nullptr, 0));
        else return HERBST_INVALID_ARGUMENT;
    }
    return 0;
}

int jumpto_command(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto client = get_client(argv[1]);
    if (client) {
        focus_client(client, true, true);
        return 0;
    } else {
        output << argv[0] << ": Could not find client";
        if (argc > 1) {
            output << " \"" << argv[1] << "\".\n";
        } else {
            output << ".\n";
        }
        return HERBST_INVALID_ARGUMENT;
    }
}

int getenv_command(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    char* envvar = getenv(argv[1]);
    if (!envvar) {
        return HERBST_ENV_UNSET;
    }
    output << envvar << "\n";
    return 0;
}

int setenv_command(int argc, char** argv, Output output) {
    if (argc < 3) {
        return HERBST_NEED_MORE_ARGS;
    }
    if (setenv(argv[1], argv[2], 1) != 0) {
        output << argv[0] << ": Could not set environment variable: " << strerror(errno) << "\n";
        return HERBST_UNKNOWN_ERROR;
    }
    return 0;
}

int unsetenv_command(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    if (unsetenv(argv[1]) != 0) {
        output << argv[0] << ": Could not unset environment variable: " << strerror(errno) << "\n";
        return HERBST_UNKNOWN_ERROR;
    }
    return 0;
}

// handle x-events:

void event_on_configure(Root*, XEvent event) {
    XConfigureRequestEvent* cre = &event.xconfigurerequest;
    Client* client = get_client_from_window(cre->window);
    if (client) {
        bool changes = false;
        auto newRect = client->float_size_;
        if (client->sizehints_floating_ &&
            (client->is_client_floated() || client->pseudotile_))
        {
            bool width_requested = 0 != (cre->value_mask & CWWidth);
            bool height_requested = 0 != (cre->value_mask & CWHeight);
            bool x_requested = 0 != (cre->value_mask & CWX);
            bool y_requested = 0 != (cre->value_mask & CWY);
            cre->width += 2*cre->border_width;
            cre->height += 2*cre->border_width;
            if (width_requested && newRect.width  != cre->width) changes = true;
            if (height_requested && newRect.height != cre->height) changes = true;
            if (x_requested || y_requested) changes = true;
            if (x_requested) newRect.x = cre->x;
            if (y_requested) newRect.y = cre->y;
            if (width_requested) newRect.width = cre->width;
            if (height_requested) newRect.height = cre->height;
        }
        if (changes && client->is_client_floated()) {
            client->float_size_ = newRect;
            client->resize_floating(find_monitor_with_tag(client->tag()), client == get_current_client());
        } else if (changes && client->pseudotile_) {
            client->float_size_ = newRect;
            Monitor* m = find_monitor_with_tag(client->tag());
            if (m) m->applyLayout();
        } else {
        // FIXME: why send event and not XConfigureWindow or XMoveResizeWindow??
            client->send_configure();
        }
    } else {
        // if client not known.. then allow configure.
        // its probably a nice conky or dzen2 bar :)
        XWindowChanges wc;
        wc.x = cre->x;
        wc.y = cre->y;
        wc.width = cre->width;
        wc.height = cre->height;
        wc.border_width = cre->border_width;
        wc.sibling = cre->above;
        wc.stack_mode = cre->detail;
        XConfigureWindow(g_display, cre->window, cre->value_mask, &wc);
    }
}

// scan for windows and add them to the list of managed clients
// from dwm.c
void scan(Root* root) {
    unsigned int num;
    Window d1, d2, *cl, *wins = nullptr;
    unsigned long cl_count;
    XWindowAttributes wa;
    auto clientmanager = root->clients();

    ewmh_get_original_client_list(&cl, &cl_count);
    if (XQueryTree(g_display, g_root, &d1, &d2, &wins, &num)) {
        for (unsigned i = 0; i < num; i++) {
            if(!XGetWindowAttributes(g_display, wins[i], &wa)
            || wa.override_redirect || XGetTransientForHint(g_display, wins[i], &d1))
                continue;
            // only manage mapped windows.. no strange wins like:
            //      luakit/dbus/(ncurses-)vim
            // but manage it if it was in the ewmh property _NET_CLIENT_LIST by
            // the previous window manager
            // TODO: what would dwm do?
            if (is_window_mapped(g_display, wins[i])
                || 0 <= array_find(cl, cl_count, sizeof(Window), wins+i)) {
                clientmanager->manage_client(wins[i], true);
                XMapWindow(g_display, wins[i]);
            }
        }
        if(wins)
            XFree(wins);
    }
    // ensure every original client is managed again
    for (unsigned i = 0; i < cl_count; i++) {
        if (get_client_from_window(cl[i])) continue;
        if (!XGetWindowAttributes(g_display, cl[i], &wa)
            || wa.override_redirect
            || XGetTransientForHint(g_display, cl[i], &d1))
        {
            continue;
        }
        XReparentWindow(g_display, cl[i], g_root, 0,0);
        clientmanager->manage_client(cl[i], true);
    }
}

void execute_autostart_file() {
    std::string path;
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
                g_warning("Will not run autostart file. "
                          "Neither $HOME or $XDG_CONFIG_HOME is set.\n");
                return;
            }
            path = std::string(home) + "/.config";
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
        HSDebug("Can not execute %s, falling back to %s\n", path.c_str(), global_autostart);
        execl(global_autostart, global_autostart, nullptr);

        fprintf(stderr, "herbstluftwm: execvp \"%s\"", global_autostart);
        perror(" failed");
        exit(EXIT_FAILURE);
    }
}

static void parse_arguments(int argc, char** argv, Globals& g) {
    static struct option long_options[] = {
        {"autostart",   1, nullptr, 'c'},
        {"version",     0, nullptr, 'v'},
        {"locked",      0, nullptr, 'l'},
        {"verbose",     0, &g_verbose, 1},
        {}
    };
    // parse options
    while (true) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "+c:vl", long_options, &option_index);
        if (c == -1) break;
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
            default:
                exit(EXIT_FAILURE);
        }
    }
}

static void remove_zombies(int) {
    int bgstatus;
    while (waitpid(-1, &bgstatus, WNOHANG) > 0);
}

static void handle_signal(int signal) {
    HSDebug("Interrupted by signal %d\n", signal);
    g_aboutToQuit = true;
    return;
}

static void sigaction_signal(int signum, void (*handler)(int)) {
    struct sigaction act;
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NOCLDSTOP | SA_RESTART;
    sigaction(signum, &act, nullptr);
}

static HandlerTable g_default_handler;

static void init_handler_table() {
    g_default_handler[ ButtonPress       ] = buttonpress;
    g_default_handler[ ButtonRelease     ] = buttonrelease;
    g_default_handler[ ClientMessage     ] = ewmh_handle_client_message;
    g_default_handler[ ConfigureNotify   ] = configurenotify;
    g_default_handler[ ConfigureRequest  ] = configurerequest;
    g_default_handler[ CreateNotify      ] = createnotify;
    g_default_handler[ DestroyNotify     ] = destroynotify;
    g_default_handler[ EnterNotify       ] = enternotify;
    g_default_handler[ Expose            ] = expose;
    g_default_handler[ FocusIn           ] = focusin;
    g_default_handler[ KeyPress          ] = keypress;
    g_default_handler[ MapNotify         ] = mapnotify;
    g_default_handler[ MapRequest        ] = maprequest;
    g_default_handler[ MappingNotify     ] = mappingnotify;
    g_default_handler[ MotionNotify      ] = motionnotify;
    g_default_handler[ PropertyNotify    ] = propertynotify;
    g_default_handler[ UnmapNotify       ] = unmapnotify;
}

static struct {
    void (*init)();
    void (*destroy)();
} g_modules[] = {
    { clientlist_init,  clientlist_destroy  },
    { ewmh_init,        ewmh_destroy        },
    { mouse_init,       mouse_destroy       },
    { hook_init,        hook_destroy        },
};

/* ----------------------------- */
/* event handler implementations */
/* ----------------------------- */

void buttonpress(Root* root, XEvent* event) {
    XButtonEvent* be = &(event->xbutton);
    HSDebug("name is: ButtonPress on sub %lx, win %lx\n", be->subwindow, be->window);
    if (mouse_binding_find(be->state, be->button)) {
        mouse_handle_event(event);
    } else {
        Client* client = get_client_from_window(be->window);
        if (client) {
            focus_client(client, false, true);
            if (root->settings->raise_on_click()) {
                    client->raise();
            }
        }
    }
    XAllowEvents(g_display, ReplayPointer, be->time);
}

void buttonrelease(Root*, XEvent*) {
    HSDebug("name is: ButtonRelease\n");
    mouse_stop_drag();
}

void createnotify(Root*, XEvent* event) {
    // printf("name is: CreateNotify\n");
    if (is_ipc_connectable(event->xcreatewindow.window)) {
        ipc_add_connection(event->xcreatewindow.window);
    }
}

void configurerequest(Root* root, XEvent* event) {
    HSDebug("name is: ConfigureRequest\n");
    event_on_configure(root, *event);
}

void configurenotify(Root* root, XEvent* event) {
    if (event->xconfigure.window == g_root &&
        root->settings->auto_detect_monitors()) {
        const char* args[] = { "detect_monitors" };
        std::ostringstream void_output;
        detect_monitors_command(LENGTH(args), args, void_output);
    }
    // HSDebug("name is: ConfigureNotify\n");
}

void destroynotify(Root* root, XEvent* event) {
    // try to unmanage it
    //HSDebug("name is: DestroyNotify for %lx\n", event->xdestroywindow.window);
    auto cm = root->clients();
    auto client = cm->client(event->xunmap.window);
    if (client) cm->force_unmanage(client);
}

void enternotify(Root* root, XEvent* event) {
    XCrossingEvent *ce = &event->xcrossing;
    //HSDebug("name is: EnterNotify, focus = %d\n", event->xcrossing.focus);
    if (!mouse_is_dragging()
        && root->settings()->focus_follows_mouse()
        && ce->focus == false) {
        Client* c = get_client_from_window(ce->window);
        std::shared_ptr<HSFrameLeaf> target;
        if (c && c->tag()->floating == false
              && (target = c->tag()->frame->frameWithClient(c))
              && target->getLayout() == LAYOUT_MAX
              && target->focusedClient() != c) {
            // don't allow focus_follows_mouse if another window would be
            // hidden during that focus change (which only occurs in max layout)
        } else if (c) {
            focus_client(c, false, true);
        }
    }
}

void expose(Root* root, XEvent* event) {
    //if (event->xexpose.count > 0) return;
    //Window ewin = event->xexpose.window;
    //HSDebug("name is: Expose for window %lx\n", ewin);
}

void focusin(Root* root, XEvent* event) {
    //HSDebug("name is: FocusIn\n");
}

void keypress(Root* root, XEvent* event) {
    //HSDebug("name is: KeyPress\n");
    root->keys()->handleKeyPress(event);
}

void mappingnotify(Root* root, XEvent* event) {
    {
        // regrab when keyboard map changes
        XMappingEvent *ev = &event->xmapping;
        XRefreshKeyboardMapping(ev);
        if(ev->request == MappingKeyboard) {
            root->keys()->regrabAll();
            //TODO: mouse_regrab_all();
        }
    }
}

void motionnotify(Root*, XEvent* event) {
    handle_motion_event(event);
}

void mapnotify(Root*, XEvent* event) {
    //HSDebug("name is: MapNotify\n");
    Client* c;
    if ((c = get_client_from_window(event->xmap.window))) {
        // reset focus. so a new window gets the focus if it shall have the
        // input focus
        // TODO: reset input focus
        //frame_focus_recursive(get_current_monitor()->tag->frame->getFocusedFrame());
        // also update the window title - just to be sure
        c->update_title();
    }
}

void maprequest(Root* root, XEvent* event) {
    HSDebug("name is: MapRequest\n");
    XMapRequestEvent* mapreq = &event->xmaprequest;
    if (is_herbstluft_window(g_display, mapreq->window)) {
        // just map the window if it wants that
        XWindowAttributes wa;
        if (!XGetWindowAttributes(g_display, mapreq->window, &wa)) {
            return;
        }
        XMapWindow(g_display, mapreq->window);
    } else if (!get_client_from_window(mapreq->window)) {
        // client should be managed (is not ignored)
        // but is not managed yet
        auto clientmanager = root->clients();
        auto client = clientmanager->manage_client(mapreq->window, false);
        if (client && find_monitor_with_tag(client->tag())) {
            XMapWindow(g_display, mapreq->window);
        }
    }
    // else: ignore all other maprequests from windows
    // that are managed already
}

void propertynotify(Root*, XEvent* event) {
    // printf("name is: PropertyNotify\n");
    XPropertyEvent *ev = &event->xproperty;
    Client* client;
    if (ev->state == PropertyNewValue) {
        if (is_ipc_connectable(event->xproperty.window)) {
            ipc_handle_connection(event->xproperty.window);
        } else if((client = get_client_from_window(ev->window))) {
            if (ev->atom == XA_WM_HINTS) {
                client->update_wm_hints();
            } else if (ev->atom == XA_WM_NORMAL_HINTS) {
                client->updatesizehints();
                Monitor* m = find_monitor_with_tag(client->tag());
                if (m) m->applyLayout();
            } else if (ev->atom == XA_WM_NAME ||
                       ev->atom == g_netatom[NetWmName]) {
                client->update_title();
            }
        }
    }
}

void unmapnotify(Root* root, XEvent* event) {
    HSDebug("name is: UnmapNotify for %lx\n", event->xunmap.window);
    root->clients()->unmap_notify(event->xunmap.window);
}

/* ---- */
/* main */
/* ---- */

int main(int argc, char* argv[]) {
    Globals g;
    parse_arguments(argc, argv, g);
    XConnection* X = XConnection::connect();
    g_display = X->display();
    if (!g_display) {
        std::cerr << "herbstluftwm: cannot open display" << std::endl;
        exit(EXIT_FAILURE);
    }
    if (X->checkotherwm()) {
        std::cerr << "herbstluftwm: another window manager is already running" << std::endl;
        exit(EXIT_FAILURE);
    }
    // remove zombies on SIGCHLD
    sigaction_signal(SIGCHLD, remove_zombies);
    sigaction_signal(SIGINT,  handle_signal);
    sigaction_signal(SIGQUIT, handle_signal);
    sigaction_signal(SIGTERM, handle_signal);
    // set some globals
    g_screen = X->screen();
    g_screen_width = X->screenWidth();
    g_screen_height = X->screenHeight();
    g_root = X->root();
    XSelectInput(g_display, g_root, ROOT_EVENT_MASK);

    auto root = std::make_shared<Root>(g);
    Root::setRoot(root);
    //test_object_system();

    init_handler_table();
    Commands::initialize(commands(root));


    // initialize subsystems
    for (unsigned i = 0; i < LENGTH(g_modules); i++) {
        g_modules[i].init();
    }

    // setup
    root->monitors()->ensure_monitors_are_available();
    scan(&* root);
    tag_force_update_flags();
    all_monitors_apply_layout();
    ewmh_update_all();
    execute_autostart_file();

    // main loop
    XEvent event;
    int x11_fd;
    fd_set in_fds;
    x11_fd = ConnectionNumber(g_display);
    while (!g_aboutToQuit) {
        FD_ZERO(&in_fds);
        FD_SET(x11_fd, &in_fds);
        // wait for an event or a signal
        select(x11_fd + 1, &in_fds, nullptr, nullptr, nullptr);
        if (g_aboutToQuit) {
            break;
        }
        XSync(g_display, False);
        while (XQLength(g_display)) {
            XNextEvent(g_display, &event);
            void (*handler) (Root*,XEvent*) = g_default_handler[event.type];
            if (handler != nullptr) {
                handler(&* root, &event);
            }
            XSync(g_display, False);
        }
    }

    // destroy all subsystems
    for (int i = LENGTH(g_modules); i --> 0;) {
        g_modules[i].destroy();
    }
    // enforce to clear the root
    root.reset();
    Root::setRoot(root);
    // and then close the x connection
    delete X;
    // check if we shall restart an other window manager
    if (g_exec_before_quit) {
        if (g_exec_args) {
            // do actual exec
            HSDebug("==> Doing wmexec to %s\n", g_exec_args[0]);
            execvp(g_exec_args[0], g_exec_args);
            fprintf(stderr, "herbstluftwm: execvp \"%s\"", g_exec_args[0]);
            perror(" failed");
        }
        // on failure or if no other wm given, then fall back
        HSDebug("==> Doing wmexec to %s\n", argv[0]);
        execvp(argv[0], argv);
        fprintf(stderr, "herbstluftwm: execvp \"%s\"", argv[1]);
        perror(" failed");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

