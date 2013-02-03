/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

// herbstluftwm
#include "clientlist.h"
#include "utils.h"
#include "key.h"
#include "layout.h"
#include "globals.h"
#include "ipc-server.h"
#include "ipc-protocol.h"
#include "command.h"
#include "settings.h"
#include "hook.h"
#include "mouse.h"
#include "rules.h"
#include "ewmh.h"
#include "stack.h"
// standard
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <assert.h>
#include <errno.h>
// gui
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>

static Bool     g_otherwm;
int g_verbose = 0;
static int (*g_xerrorxlib)(Display *, XErrorEvent *);
static char*    g_autostart_path = NULL; // if not set, then find it in $HOME or $XDG_CONFIG_HOME
static int*     g_focus_follows_mouse = NULL;
static bool     g_exec_before_quit = false;
static char**   g_exec_args = NULL;
static int*     g_raise_on_click = NULL;

typedef void (*HandlerTable[LASTEvent]) (XEvent*);

int quit();
int reload();
int version(int argc, char* argv[], GString* output);
int echo(int argc, char* argv[], GString* output);
int print_layout_command(int argc, char** argv, GString* output);
int load_command(int argc, char** argv, GString* output);
int print_tag_status_command(int argc, char** argv, GString* output);
void execute_autostart_file();
int raise_command(int argc, char** argv, GString* output);
int spawn(int argc, char** argv);
int wmexec(int argc, char** argv);
static void remove_zombies(int signal);
int custom_hook_emit(int argc, char** argv);
int jumpto_command(int argc, char** argv, GString* output);
int getenv_command(int argc, char** argv, GString* output);
int setenv_command(int argc, char** argv, GString* output);
int unsetenv_command(int argc, char** argv, GString* output);

// handler for X-Events
void buttonpress(XEvent* event);
void buttonrelease(XEvent* event);
void createnotify(XEvent* event);
void configurerequest(XEvent* event);
void configurenotify(XEvent* event);
void destroynotify(XEvent* event);
void enternotify(XEvent* event);
void expose(XEvent* event);
void focusin(XEvent* event);
void keypress(XEvent* event);
void mappingnotify(XEvent* event);
void motionnotify(XEvent* event);
void mapnotify(XEvent* event);
void maprequest(XEvent* event);
void propertynotify(XEvent* event);
void unmapnotify(XEvent* event);

CommandBinding g_commands[] = {
    CMD_BIND_NO_OUTPUT(   "quit",           quit),
    CMD_BIND(             "echo",           echo),
    CMD_BIND_NO_OUTPUT(   "reload",         reload),
    CMD_BIND(             "version",        version),
    CMD_BIND(             "list_commands",  list_commands),
    CMD_BIND(             "list_monitors",  list_monitors),
    CMD_BIND(             "set_monitors",   set_monitor_rects_command),
    CMD_BIND(             "disjoin_rects",  disjoin_rects_command),
    CMD_BIND(             "list_keybinds",  key_list_binds),
    CMD_BIND(             "list_padding",   list_padding),
    CMD_BIND(             "keybind",        keybind),
    CMD_BIND(             "keyunbind",      keyunbind),
    CMD_BIND(             "mousebind",      mouse_bind_command),
    CMD_BIND_NO_OUTPUT(   "mouseunbind",    mouse_unbind_all),
    CMD_BIND_NO_OUTPUT(   "spawn",          spawn),
    CMD_BIND_NO_OUTPUT(   "wmexec",         wmexec),
    CMD_BIND_NO_OUTPUT(   "emit_hook",      custom_hook_emit),
    CMD_BIND(             "bring",          frame_current_bring),
    CMD_BIND_NO_OUTPUT(   "focus_nth",      frame_current_set_selection),
    CMD_BIND_NO_OUTPUT(   "cycle",          frame_current_cycle_selection),
    CMD_BIND_NO_OUTPUT(   "cycle_all",      cycle_all_command),
    CMD_BIND(             "cycle_layout",   frame_current_cycle_client_layout),
    CMD_BIND_NO_OUTPUT(   "close",          window_close_current),
    CMD_BIND_NO_OUTPUT(   "close_or_remove",close_or_remove_command),
    CMD_BIND(             "split",          frame_split_command),
    CMD_BIND(             "resize",         frame_change_fraction_command),
    CMD_BIND(             "focus_edge",     frame_focus_edge),
    CMD_BIND(             "focus",          frame_focus_command),
    CMD_BIND(             "shift_edge",     frame_move_window_edge),
    CMD_BIND(             "shift",          frame_move_window_command),
    CMD_BIND(             "shift_to_monitor",shift_to_monitor),
    CMD_BIND_NO_OUTPUT(   "remove",         frame_remove_command),
    CMD_BIND(             "set",            settings_set_command),
    CMD_BIND(             "toggle",         settings_toggle),
    CMD_BIND(             "cycle_value",    settings_cycle_value),
    CMD_BIND_NO_OUTPUT(   "cycle_monitor",  monitor_cycle_command),
    CMD_BIND(             "focus_monitor",  monitor_focus_command),
    CMD_BIND(             "get",            settings_get),
    CMD_BIND(             "add",            tag_add_command),
    CMD_BIND(             "use",            monitor_set_tag_command),
    CMD_BIND(             "use_index",      monitor_set_tag_by_index_command),
    CMD_BIND(             "use_previous",   monitor_set_previous_tag_command),
    CMD_BIND(             "jumpto",         jumpto_command),
    CMD_BIND(             "floating",       tag_set_floating_command),
    CMD_BIND_NO_OUTPUT(   "fullscreen",     client_set_property_command),
    CMD_BIND_NO_OUTPUT(   "pseudotile",     client_set_property_command),
    CMD_BIND(             "tag_status",     print_tag_status_command),
    CMD_BIND(             "merge_tag",      tag_remove_command),
    CMD_BIND(             "rename",         tag_rename_command),
    CMD_BIND(             "move",           tag_move_window_command),
    CMD_BIND_NO_OUTPUT(   "rotate",         layout_rotate_command),
    CMD_BIND(             "move_index",     tag_move_window_by_index_command),
    CMD_BIND(             "add_monitor",    add_monitor_command),
    CMD_BIND(             "raise_monitor",  monitor_raise_command),
    CMD_BIND(             "remove_monitor", remove_monitor_command),
    CMD_BIND(             "move_monitor",   move_monitor_command),
    CMD_BIND(             "rename_monitor", rename_monitor_command),
    CMD_BIND(             "monitor_rect",   monitor_rect_command),
    CMD_BIND(             "pad",            monitor_set_pad_command),
    CMD_BIND(             "raise",          raise_command),
    CMD_BIND(             "rule",           rule_add_command),
    CMD_BIND(             "unrule",         rule_remove_command),
    CMD_BIND(             "layout",         print_layout_command),
    CMD_BIND(             "stack",          print_stack_command),
    CMD_BIND(             "dump",           print_layout_command),
    CMD_BIND(             "load",           load_command),
    CMD_BIND(             "complete",       complete_command),
    CMD_BIND(             "complete_shell", complete_command),
    CMD_BIND_NO_OUTPUT(   "lock",           monitors_lock_command),
    CMD_BIND_NO_OUTPUT(   "unlock",         monitors_unlock_command),
    CMD_BIND(             "lock_tag",       monitor_lock_tag_command),
    CMD_BIND(             "unlock_tag",     monitor_unlock_tag_command),
    CMD_BIND(             "set_layout",     frame_current_set_client_layout),
    CMD_BIND(             "detect_monitors",detect_monitors_command),
    CMD_BIND(             "chain",          command_chain_command),
    CMD_BIND(             "and",            command_chain_command),
    CMD_BIND(             "or",             command_chain_command),
    CMD_BIND(             "!",              negate_command),
    CMD_BIND(             "getenv",         getenv_command),
    CMD_BIND(             "setenv",         setenv_command),
    CMD_BIND(             "unsetenv",       unsetenv_command),
    {{ NULL }}
};

// core functions
int quit() {
    g_aboutToQuit = true;
    return 0;
}

// reload config
int reload() {
    execute_autostart_file();
    return 0;
}

int version(int argc, char* argv[], GString* output) {
    (void) argc;
    (void) argv;
    g_string_append(output, HERBSTLUFT_VERSION_STRING);
    return 0;
}

int echo(int argc, char* argv[], GString* output) {
    if (SHIFT(argc, argv)) {
        // if there still is an argument
        g_string_append(output, argv[0]);
        while (SHIFT(argc, argv)) {
            g_string_append_c(output, ' ');
            g_string_append(output, argv[0]);
        }
    }
    g_string_append_c(output, '\n');
    return 0;
}

// prints or dumps the layout of an given tag
// first argument tells whether to print or to dump
int print_layout_command(int argc, char** argv, GString* output) {
    HSTag* tag = NULL;
    // an empty argv[1] means current focused tag
    if (argc >= 2 && argv[1][0] != '\0') {
        tag = find_tag(argv[1]);
        if (!tag) {
            g_string_append_printf(output,
                "%s: Tag \"%s\" not found\n", argv[0], argv[1]);
            return HERBST_INVALID_ARGUMENT;
        }
    } else { // use current tag
        HSMonitor* m = get_current_monitor();
        tag = m->tag;
    }
    assert(tag != NULL);

    HSFrame* frame = lookup_frame(tag->frame, argc >= 3 ? argv[2] : "");
    if (argc > 0 && !strcmp(argv[0], "dump")) {
        dump_frame_tree(frame, output);
    } else {
        print_frame_tree(frame, output);
    }
    return 0;
}

int load_command(int argc, char** argv, GString* output) {
    // usage: load TAG LAYOUT
    HSTag* tag = NULL;
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    char* layout_string = argv[1];
    if (argc >= 3) {
        tag = find_tag(argv[1]);
        layout_string = argv[2];
        if (!tag) {
            g_string_append_printf(output,
                "%s: Tag \"%s\" not found\n", argv[0], argv[1]);
            return HERBST_INVALID_ARGUMENT;
        }
    } else { // use current tag
        HSMonitor* m = get_current_monitor();
        tag = m->tag;
    }
    assert(tag != NULL);
    char* rest = load_frame_tree(tag->frame, layout_string, output);
    if (output->len > 0) {
        g_string_prepend(output, "load: ");
    }
    tag_set_flags_dirty(); // we probably changed some window positions
    // arrange monitor
    HSMonitor* m = find_monitor_with_tag(tag);
    if (m) {
        frame_show_recursive(tag->frame);
        if (get_current_monitor() == m) {
            frame_focus_recursive(tag->frame);
        }
        monitor_apply_layout(m);
    } else {
        frame_hide_recursive(tag->frame);
    }
    if (!rest) {
        g_string_append_printf(output,
            "%s: Error while parsing!\n", argv[0]);
        return HERBST_INVALID_ARGUMENT;
    }
    if (rest[0] != '\0') { // if string was not parsed completely
        g_string_append_printf(output,
            "%s: Layout description was too long\n", argv[0]);
        g_string_append_printf(output,
            "%s: \"%s\" has not been parsed\n", argv[0], rest);
        return HERBST_INVALID_ARGUMENT;
    }
    return 0;
}

int print_tag_status_command(int argc, char** argv, GString* output) {
    HSMonitor* monitor;
    if (argc >= 2) {
        monitor = string_to_monitor(argv[1]);
    } else {
        monitor = get_current_monitor();
    }
    if (monitor == NULL) {
        g_string_append_printf(output,
            "%s: Monitor \"%s\" not found!\n", argv[0], argv[1]);
        return HERBST_INVALID_ARGUMENT;
    }
    tag_update_flags();
    g_string_append_c(output, '\t');
    for (int i = 0; i < g_tags->len; i++) {
        HSTag* tag = g_array_index(g_tags, HSTag*, i);
        // print flags
        char c = '.';
        if (tag->flags & TAG_FLAG_USED) {
            c = ':';
        }
        HSMonitor *tag_monitor = find_monitor_with_tag(tag);
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
        g_string_append_c(output, c);
        g_string_append(output, tag->name->str);
        g_string_append_c(output, '\t');
    }
    return 0;
}

int custom_hook_emit(int argc, char** argv) {
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
        execargs[i] = NULL;
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
        execargs[i] = NULL;
        // quit and exec to new window manger
        g_exec_args = execargs;
    } else {
        // exec into same command
        g_exec_args = NULL;
    }
    g_exec_before_quit = true;
    g_aboutToQuit = true;
    return EXIT_SUCCESS;
}

int raise_command(int argc, char** argv, GString* output) {
    Window win;
    HSClient* client = NULL;
    win = string_to_client((argc > 1) ? argv[1] : "", &client);
    if (client) {
        client_raise(client);
    } else {
        XRaiseWindow(g_display, win);
    }
    return 0;
}

int jumpto_command(int argc, char** argv, GString* output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    HSClient* client = NULL;
    string_to_client(argv[1], &client);
    if (client) {
        focus_window(client->window, true, true);
        return 0;
    } else {
        g_string_append_printf(output,
            "%s: Could not find client", argv[0]);
        if (argc > 1) {
            g_string_append_printf(output, " \"%s\".\n", argv[1]);
        } else {
            g_string_append(output, ".\n");
        }
        return HERBST_INVALID_ARGUMENT;
    }
}

int getenv_command(int argc, char** argv, GString* output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    char* envvar = getenv(argv[1]);
    if (envvar == NULL) {
        return HERBST_ENV_UNSET;
    }
    g_string_append_printf(output, "%s\n", envvar);
    return 0;
}

int setenv_command(int argc, char** argv, GString* output) {
    if (argc < 3) {
        return HERBST_NEED_MORE_ARGS;
    }
    if (setenv(argv[1], argv[2], 1) != 0) {
        g_string_append_printf(output,
            "%s: Could not set environment variable: %s\n", argv[0], strerror(errno));
        return HERBST_UNKNOWN_ERROR;
    }
    return 0;
}

int unsetenv_command(int argc, char** argv, GString* output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    if (unsetenv(argv[1]) != 0) {
        g_string_append_printf(output,
            "%s: Could not unset environment variable: %s\n", argv[0], strerror(errno));
        return HERBST_UNKNOWN_ERROR;
    }
    return 0;
}

// handle x-events:

void event_on_configure(XEvent event) {
    XConfigureRequestEvent* cre = &event.xconfigurerequest;
    HSClient* client = get_client_from_window(cre->window);
    XConfigureEvent ce;
    ce.type = ConfigureNotify;
    ce.display = g_display;
    ce.event = cre->window;
    ce.window = cre->window;
    if (client) {
        cre->width += 2*cre->border_width - 2*client->last_border_width;
        cre->height += 2*cre->border_width - 2*client->last_border_width;
        client->float_size.width = cre->width;
        client->float_size.height = cre->height;
        ce.x = client->last_size.x;
        ce.y = client->last_size.y;
        ce.width = client->last_size.width;
        ce.height = client->last_size.height;
        ce.override_redirect = False;
        ce.border_width = cre->border_width;
        ce.above = cre->above;
        if (client->tag->floating || client->pseudotile) {
            monitor_apply_layout(find_monitor_with_tag(client->tag));
        } else {
        // FIXME: why send event and not XConfigureWindow or XMoveResizeWindow??
            XSendEvent(g_display, cre->window, False, StructureNotifyMask,
                       (XEvent*)&ce);
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

// from dwm.c
/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's).  Other types of errors call Xlibs
 * default error handler, which may call exit.  */
int xerror(Display *dpy, XErrorEvent *ee) {
    if(ee->error_code == BadWindow
    || ee->error_code == BadGC
    || ee->error_code == BadPixmap
    || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
    || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
    || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
    || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
    || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
    || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable)) {
        return 0;
    }
    fprintf(stderr, "herbstluftwm: fatal error: request code=%d, error code=%d\n",
            ee->request_code, ee->error_code);
    if (ee->error_code == BadDrawable) {
        HSDebug("Warning: ignoring X_BadDrawable");
        return 0;
    }
    return g_xerrorxlib(dpy, ee); /* may call exit */
}

int xerrordummy(Display *dpy, XErrorEvent *ee) {
    return 0;
}

// from dwm.c
/* Startup Error handler to check if another window manager
 * is already running. */
int xerrorstart(Display *dpy, XErrorEvent *ee) {
    g_otherwm = True;
    return -1;
}

// from dwm.c
void checkotherwm(void) {
    g_otherwm = False;
    g_xerrorxlib = XSetErrorHandler(xerrorstart);
    /* this causes an error if some other window manager is running */
    XSelectInput(g_display, DefaultRootWindow(g_display), SubstructureRedirectMask);
    XSync(g_display, False);
    if(g_otherwm)
        die("herbstluftwm: another window manager is already running\n");
    XSetErrorHandler(xerror);
    XSync(g_display, False);
}

// scan for windows and add them to the list of managed clients
// from dwm.c
void scan(void) {
    unsigned int num;
    Window d1, d2, *cl, *wins = NULL;
    unsigned long cl_count;
    XWindowAttributes wa;

    ewmh_get_original_client_list(&cl, &cl_count);
    if (XQueryTree(g_display, g_root, &d1, &d2, &wins, &num)) {
        for (int i = 0; i < num; i++) {
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
                manage_client(wins[i]);
                XMapWindow(g_display, wins[i]);
            }
        }
        if(wins)
            XFree(wins);
    }
}

void execute_autostart_file() {
    GString* path = NULL;
    if (g_autostart_path) {
        path = g_string_new(g_autostart_path);
    } else {
        // find right directory
        char* xdg_config_home = getenv("XDG_CONFIG_HOME");
        if (xdg_config_home) {
            path = g_string_new(xdg_config_home);
        } else {
            char* home = getenv("HOME");
            if (!home) {
                g_warning("Will not run autostart file. "
                          "Neither $HOME or $XDG_CONFIG_HOME is set.\n");
                return;
            }
            path = g_string_new(home);
            g_string_append_c(path, G_DIR_SEPARATOR);
            g_string_append(path, ".config");
        }
        g_string_append_c(path, G_DIR_SEPARATOR);
        g_string_append(path, HERBSTLUFT_AUTOSTART);
    }
    if (0 == fork()) {
        if (g_display) {
            close(ConnectionNumber(g_display));
        }
        setsid();
        execl(path->str, path->str, NULL);

        char* global_autostart = HERBSTLUFT_GLOBAL_AUTOSTART;
        HSDebug("Can not execute %s, falling back to %s\n", path->str, global_autostart);
        execl(global_autostart, global_autostart, NULL);

        fprintf(stderr, "herbstluftwm: execvp \"%s\"", global_autostart);
        perror(" failed");
        exit(EXIT_FAILURE);
    }
    g_string_free(path, true);
}

static void parse_arguments(int argc, char** argv) {
    static struct option long_options[] = {
        {"autostart",   1, 0, 'c'},
        {"version",     0, 0, 'v'},
        {"locked",      0, 0, 'l'},
        {"verbose",     0, &g_verbose, 1},
        {0, 0, 0, 0}
    };
    // parse options
    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "+c:vl", long_options, &option_index);
        if (c == -1) break;
        switch (c) {
            case 0:
                /* ignore recognized long option */
                break;
            case 'v':
                printf("%s %s\n", argv[0], HERBSTLUFT_VERSION);
                printf("Copyright (c) 2011-2013 Thorsten Wißmann\n");
                printf("Released under the Simplified BSD License\n");
                exit(0);
                break;
            case 'c':
                g_autostart_path = optarg;
                break;
            case 'l':
                g_initial_monitors_locked = 1;
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }
}

static void remove_zombies(int signal) {
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
    sigaction(signum, &act, NULL);
}

static void fetch_settings() {
    // fetch settings only for this main.c file from settings table
    g_focus_follows_mouse = &(settings_find("focus_follows_mouse")->value.i);
    g_raise_on_click = &(settings_find("raise_on_click")->value.i);
}

static HandlerTable g_default_handler = {
    [ ButtonPress       ] = buttonpress,
    [ ButtonRelease     ] = buttonrelease,
    [ ClientMessage     ] = ewmh_handle_client_message,
    [ CreateNotify      ] = createnotify,
    [ ConfigureRequest  ] = configurerequest,
    [ ConfigureNotify   ] = configurenotify,
    [ DestroyNotify     ] = destroynotify,
    [ EnterNotify       ] = enternotify,
    [ Expose            ] = expose,
    [ FocusIn           ] = focusin,
    [ KeyPress          ] = keypress,
    [ MappingNotify     ] = mappingnotify,
    [ MotionNotify      ] = motionnotify,
    [ MapNotify         ] = mapnotify,
    [ MapRequest        ] = maprequest,
    [ PropertyNotify    ] = propertynotify,
    [ UnmapNotify       ] = unmapnotify,
};

static struct {
    void (*init)();
    void (*destroy)();
} g_modules[] = {
    { ipc_init,         ipc_destroy         },
    { key_init,         key_destroy         },
    { settings_init,    settings_destroy    },
    { stacklist_init,   stacklist_destroy   },
    { layout_init,      layout_destroy      },
    { tag_init,         tag_destroy         },
    { clientlist_init,  clientlist_destroy  },
    { monitor_init,     monitor_destroy     },
    { ewmh_init,        ewmh_destroy        },
    { mouse_init,       mouse_destroy       },
    { hook_init,        hook_destroy        },
    { rules_init,       rules_destroy       },
};

/* ----------------------------- */
/* event handler implementations */
/* ----------------------------- */

void buttonpress(XEvent* event) {
    XButtonEvent* be = &(event->xbutton);
    HSDebug("name is: ButtonPress on sub %lx, win %lx\n", be->subwindow, be->window);
    if (mouse_binding_find(be->state, be->button)) {
        mouse_start_drag(event);
    } else {
        focus_window(be->window, false, true);
        if (*g_raise_on_click) {
            HSClient* client = get_client_from_window(be->window);
            if (client) {
                client_raise(client);
            }
        }
    }
    XAllowEvents(g_display, ReplayPointer, be->time);
}

void buttonrelease(XEvent* event) {
    HSDebug("name is: ButtonRelease\n");
    mouse_stop_drag();
}

void createnotify(XEvent* event) {
    // printf("name is: CreateNotify\n");
    if (is_ipc_connectable(event->xcreatewindow.window)) {
        ipc_add_connection(event->xcreatewindow.window);
    }
}

void configurerequest(XEvent* event) {
    HSDebug("name is: ConfigureRequest\n");
    event_on_configure(*event);
}

void configurenotify(XEvent* event) {
    if (event->xconfigure.window == g_root &&
        settings_find("auto_detect_monitors")->value.i) {
        char* args[] = { "detect_monitors" };
        detect_monitors_command(LENGTH(args), args, NULL);
    }
    // HSDebug("name is: ConfigureNotify\n");
}

void destroynotify(XEvent* event) {
    // try to unmanage it
    unmanage_client(event->xdestroywindow.window);
}

void enternotify(XEvent* event) {
    XCrossingEvent *ce = &event->xcrossing;
    HSDebug("name is: EnterNotify, focus = %d\n", event->xcrossing.focus);
    if (!mouse_is_dragging()
        && *g_focus_follows_mouse
        && false == ce->focus) {
        HSClient* c = get_client_from_window(ce->window);
        HSFrame* target;
        if (c && c->tag->floating == false
              && (target = find_frame_with_window(c->tag->frame, ce->window))
              && target->content.clients.layout == LAYOUT_MAX
              && frame_focused_window(target) != ce->window) {
            // don't allow focus_follows_mouse if another window would be
            // hidden during that focus change (which only occurs in max layout)
        } else {
            focus_window(ce->window, false, true);
        }
    }
}

void expose(XEvent* event) {
    HSDebug("name is: Expose\n");
}

void focusin(XEvent* event) {
    HSDebug("name is: FocusIn\n");
}

void keypress(XEvent* event) {
    HSDebug("name is: KeyPress\n");
    handle_key_press(event);
}

void mappingnotify(XEvent* event) {
    {
        // regrab when keyboard map changes
        XMappingEvent *ev = &event->xmapping;
        XRefreshKeyboardMapping(ev);
        if(ev->request == MappingKeyboard) {
            regrab_keys();
            //TODO: mouse_regrab_all();
        }
    }
}

void motionnotify(XEvent* event) {
    handle_motion_event(event);
}

void mapnotify(XEvent* event) {
    HSDebug("name is: MapNotify\n");
    HSClient* c;
    if ((c = get_client_from_window(event->xmap.window))) {
        // reset focus. so a new window gets the focus if it shall have the
        // input focus
        frame_focus_recursive(g_cur_frame);
        // also update the window title - just to be sure
        client_update_title(c);
    }
}

void maprequest(XEvent* event) {
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
        HSClient* client = manage_client(mapreq->window);
        if (client && find_monitor_with_tag(client->tag)) {
            XMapWindow(g_display, mapreq->window);
        }
    }
    // else: ignore all other maprequests from windows
    // that are managed already
}

void propertynotify(XEvent* event) {
    // printf("name is: PropertyNotify\n");
    XPropertyEvent *ev = &event->xproperty;
    HSClient* client;
    if (ev->state == PropertyNewValue) {
        if (is_ipc_connectable(event->xproperty.window)) {
            ipc_handle_connection(event->xproperty.window);
        } else if((client = get_client_from_window(ev->window))) {
            switch (ev->atom) {
                case XA_WM_HINTS:
                    client_update_wm_hints(client);
                    break;
                case XA_WM_NAME:
                    client_update_title(client);
                    break;
                default:
                    break;
            }
        }
    }
}

void unmapnotify(XEvent* event) {
    HSDebug("name is: UnmapNotify for %lx\n", event->xunmap.window);
    unmanage_client(event->xunmap.window);
}

/* ---- */
/* main */
/* ---- */

int main(int argc, char* argv[]) {
    parse_arguments(argc, argv);
    if(!(g_display = XOpenDisplay(NULL)))
        die("herbstluftwm: cannot open display\n");
    checkotherwm();
    // remove zombies on SIGCHLD
    sigaction_signal(SIGCHLD, remove_zombies);
    sigaction_signal(SIGINT,  handle_signal);
    sigaction_signal(SIGQUIT, handle_signal);
    sigaction_signal(SIGTERM, handle_signal);
    // set some globals
    g_screen = DefaultScreen(g_display);
    g_screen_width = DisplayWidth(g_display, g_screen);
    g_screen_height = DisplayHeight(g_display, g_screen);
    g_root = RootWindow(g_display, g_screen);
    XSelectInput(g_display, g_root, ROOT_EVENT_MASK);

    // initialize subsystems
    for (int i = 0; i < LENGTH(g_modules); i++) {
        g_modules[i].init();
    }
    fetch_settings();

    // setup
    ensure_monitors_are_available();
    scan();
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
        select(x11_fd + 1, &in_fds, 0, 0, NULL);
        if (g_aboutToQuit) {
            break;
        }
        while (XPending(g_display)) {
            XNextEvent(g_display, &event);
            void (*handler) (XEvent*) = g_default_handler[event.type];
            if (handler != NULL) {
                handler(&event);
            }
        }
    }

    // destroy all subsystems
    for (int i = LENGTH(g_modules); i --> 0;) {
        g_modules[i].destroy();
    }
    XCloseDisplay(g_display);
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

