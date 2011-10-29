/** Copyright 2011 Thorsten Wißmann. All rights reserved.
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
// standard
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <sys/wait.h>
#include <assert.h>
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
int version(int argc, char* argv[], GString** result);
int print_layout_command(int argc, char** argv, GString** result);
int load_command(int argc, char** argv, GString** result);
int print_tag_status_command(int argc, char** argv, GString** result);
void execute_autostart_file();
int raise_command(int argc, char** argv);
int spawn(int argc, char** argv);
int wmexec(int argc, char** argv);
static void remove_zombies(int signal);
int custom_hook_emit(int argc, char** argv);

// handler for X-Events
void buttonpress(XEvent* event);
void buttonrelease(XEvent* event);
void clientmessage(XEvent* event);
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
    CMD_BIND_NO_OUTPUT(   "reload",         reload),
    CMD_BIND(             "version",        version),
    CMD_BIND(             "list_commands",  list_commands),
    CMD_BIND(             "list_monitors",  list_monitors),
    CMD_BIND_NO_OUTPUT(   "keybind",        keybind),
    CMD_BIND_NO_OUTPUT(   "keyunbind",      keyunbind),
    CMD_BIND_NO_OUTPUT(   "mousebind",      mouse_bind_command),
    CMD_BIND_NO_OUTPUT(   "mouseunbind",    mouse_unbind_all),
    CMD_BIND_NO_OUTPUT(   "spawn",          spawn),
    CMD_BIND_NO_OUTPUT(   "wmexec",         wmexec),
    CMD_BIND_NO_OUTPUT(   "emit_hook",      custom_hook_emit),
    CMD_BIND_NO_OUTPUT(   "cycle",          frame_current_cycle_selection),
    CMD_BIND_NO_OUTPUT(   "cycle_all",      cycle_all_command),
    CMD_BIND_NO_OUTPUT(   "cycle_layout",   frame_current_cycle_client_layout),
    CMD_BIND_NO_OUTPUT(   "close",          window_close_current),
    CMD_BIND_NO_OUTPUT(   "split",          frame_split_command),
    CMD_BIND_NO_OUTPUT(   "resize",         frame_change_fraction_command),
    CMD_BIND_NO_OUTPUT(   "focus",          frame_focus_command),
    CMD_BIND_NO_OUTPUT(   "shift",          frame_move_window_command),
    CMD_BIND_NO_OUTPUT(   "remove",         frame_remove_command),
    CMD_BIND_NO_OUTPUT(   "set",            settings_set),
    CMD_BIND_NO_OUTPUT(   "toggle",         settings_toggle),
    CMD_BIND_NO_OUTPUT(   "cycle_monitor",  monitor_cycle_command),
    CMD_BIND_NO_OUTPUT(   "focus_monitor",  monitor_focus_command),
    CMD_BIND(             "get",            settings_get),
    CMD_BIND_NO_OUTPUT(   "add",            tag_add_command),
    CMD_BIND_NO_OUTPUT(   "use",            monitor_set_tag_command),
    CMD_BIND(             "floating",       tag_set_floating_command),
    CMD_BIND_NO_OUTPUT(   "fullscreen",     client_set_property_command),
    CMD_BIND_NO_OUTPUT(   "pseudotile",     client_set_property_command),
    CMD_BIND(             "tag_status",     print_tag_status_command),
    CMD_BIND_NO_OUTPUT(   "merge_tag",      tag_remove_command),
    CMD_BIND_NO_OUTPUT(   "rename",         tag_rename_command),
    CMD_BIND_NO_OUTPUT(   "move",           tag_move_window_command),
    CMD_BIND_NO_OUTPUT(   "add_monitor",    add_monitor_command),
    CMD_BIND_NO_OUTPUT(   "remove_monitor", remove_monitor_command),
    CMD_BIND_NO_OUTPUT(   "move_monitor",   move_monitor_command),
    CMD_BIND(             "monitor_rect",   monitor_rect_command),
    CMD_BIND_NO_OUTPUT(   "pad",            monitor_set_pad_command),
    CMD_BIND_NO_OUTPUT(   "raise",          raise_command),
    CMD_BIND_NO_OUTPUT(   "rule",           rule_add_command),
    CMD_BIND_NO_OUTPUT(   "unrule",         rule_remove_command),
    CMD_BIND(             "layout",         print_layout_command),
    CMD_BIND(             "dump",           print_layout_command),
    CMD_BIND(             "load",           load_command),
    CMD_BIND(             "complete",       complete_command),
    {{ NULL }}
};

// core funcitons
int quit() {
    g_aboutToQuit = true;
    return 0;
}

// reload config
int reload() {
    execute_autostart_file();
    return 0;
}

int version(int argc, char* argv[], GString** result) {
    (void) argc;
    (void) argv;
    *result = g_string_assign(*result, HERBSTLUFT_VERSION_STRING);
    return 0;
}

// prints or dumps the layout of an given tag
// first argument tells wether to print or to dump
int print_layout_command(int argc, char** argv, GString** result) {
    HSTag* tag = NULL;
    if (argc >= 2) {
        tag = find_tag(argv[1]);
    }
    // if no tag was found
    if (!tag) {
        HSMonitor* m = &g_array_index(g_monitors, HSMonitor, g_cur_monitor);
        tag = m->tag;
    }
    assert(tag != NULL);
    if (argc > 0 && !strcmp(argv[0], "dump")) {
        dump_frame_tree(tag->frame, result);
    } else {
        print_tag_tree(tag, result);
    }
    return 0;
}

int load_command(int argc, char** argv, GString** result) {
    // usage: load TAG LAYOUT
    HSTag* tag = NULL;
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }
    char* layout_string = argv[1];
    if (argc >= 3) {
        tag = find_tag(argv[1]);
        layout_string = argv[2];
    }
    // if no tag was found
    if (!tag) {
        HSMonitor* m = &g_array_index(g_monitors, HSMonitor, g_cur_monitor);
        tag = m->tag;
    }
    assert(tag != NULL);
    char* rest = load_frame_tree(tag->frame, layout_string, result);
    tag_set_flags_dirty(); // we probably changed some window positions
    // arrange monitor
    HSMonitor* m = find_monitor_with_tag(tag);
    if (m) {
        frame_show_recursive(tag->frame);
        monitor_apply_layout(m);
    } else {
        frame_hide_recursive(tag->frame);
    }
    if (!rest) {
        return HERBST_INVALID_ARGUMENT;
    }
    if (rest[0] != '\0') { // if string wasnot parsed completely
        g_string_append_printf(*result,
            "%s: layout description was too long\n", argv[0]);
        g_string_append_printf(*result,
            "%s: \"%s\" has not been parsed\n", argv[0], rest);
        return HERBST_INVALID_ARGUMENT;
    }
    return 0;
}

int print_tag_status_command(int argc, char** argv, GString** result) {
    int monitor_index = g_cur_monitor;
    if (argc >= 2) {
        monitor_index = atoi(argv[1]);
    }
    monitor_index = CLAMP(monitor_index, 0, g_monitors->len);
    tag_update_flags();
    HSMonitor* monitor = &g_array_index(g_monitors, HSMonitor, monitor_index);
    *result = g_string_append_c(*result, '\t');
    for (int i = 0; i < g_tags->len; i++) {
        HSTag* tag = g_array_index(g_tags, HSTag*, i);
        // print flags
        char c = '.';
        if (tag->flags & TAG_FLAG_USED) {
            c = ':';
        }
        if (tag == monitor->tag) {
            c = '+';
            if (monitor_index == g_cur_monitor) {
                c = '#';
            }
        }
        if (tag->flags & TAG_FLAG_URGENT) {
            c = '!';
        }
        *result = g_string_append_c(*result, c);
        *result = g_string_append(*result, tag->name->str);
        *result = g_string_append_c(*result, '\t');
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
        fprintf(stderr, "spawn: too few parameters\n");
        return HERBST_INVALID_ARGUMENT;
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

int raise_command(int argc, char** argv) {
    Window win;
    if (argc > 1) {
        if (1 != sscanf(argv[1], "0x%lx", (long unsigned int*)&win)) {
            // a invalid winid was given
            return 1;
        }
    } else {
        win = frame_focused_window(g_cur_frame);
        if (!win) {
            return 0;
        }
    }
    XRaiseWindow(g_display, win);
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
        ce.x = client->last_size.x;
        ce.y = client->last_size.y;
        ce.width = client->last_size.width;
        ce.height = client->last_size.height;
        ce.override_redirect = False;
        ce.border_width = cre->border_width;
        ce.above = cre->above;
        // FIXME: why send event and not XConfigureWindow or XMoveResizeWindow??
        XSendEvent(g_display, cre->window, False, StructureNotifyMask, (XEvent*)&ce);
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
    || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
    || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
    || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
    || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
    || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
    || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
        return 0;
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
    unsigned int i, num;
    Window d1, d2, *wins = NULL;
    XWindowAttributes wa;

    if(XQueryTree(g_display, g_root, &d1, &d2, &wins, &num)) {
        for(i = 0; i < num; i++) {
            if(!XGetWindowAttributes(g_display, wins[i], &wa)
            || wa.override_redirect || XGetTransientForHint(g_display, wins[i], &d1))
                continue;
            // only manage mapped windows.. no strange wins like:
            //      luakit/dbus/(ncurses-)vim
            // TODO: what would dwm do?
            if (!is_window_ignored(wins[i]) &&
                is_window_mapped(g_display, wins[i])) {
                manage_client(wins[i]);
            }
        }
        if(wins)
            XFree(wins);
    }
}

void execute_autostart_file() {
    GString* path;
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
                g_warning("Willnot run autostart file. "
                          "Neither $HOME or $XDG_CONFIG_HOME is set.\n");
                return;
            }
            path = g_string_new(home);
            path = g_string_append_c(path, G_DIR_SEPARATOR);
            path = g_string_append(path, ".config");
        }
        path = g_string_append_c(path, G_DIR_SEPARATOR);
        path = g_string_append(path, HERBSTLUFT_AUTOSTART);
    }
    char* argv[] = {
        "...", // command name... but it doesnot matter
        path->str
    };
    spawn(LENGTH(argv), argv);
    g_string_free(path, true);
}

static void parse_arguments(int argc, char** argv) {
    static struct option long_options[] = {
        {"autostart",   1, 0, 'c'},
        {"version",     0, 0, 'v'},
        {"verbose",     0, &g_verbose, 1},
        {0, 0, 0, 0}
    };
    // parse options
    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "+c:v", long_options, &option_index);
        if (c == -1) break;
        switch (c) {
            case 0:
                /* ignore recognized long option */
                break;
            case 'v':
                printf("%s %s\n", argv[0], HERBSTLUFT_VERSION);
                printf("Copyright (c) 2011 Thorsten Wißmann\n");
                printf("Released under the Simplified BSD License\n");
                exit(0);
                break;
            case 'c':
                g_autostart_path = optarg;
                break;
            default:
                fprintf(stderr, "unknown option `%s'\n", argv[optind]);
                exit(EXIT_FAILURE);
        }
    }
}

static void remove_zombies(int signal) {
    int bgstatus;
    while (waitpid(-1, &bgstatus, WNOHANG) > 0);
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
    [ButtonPress] = buttonpress,
    [ButtonRelease] = buttonrelease,
    [ClientMessage] = clientmessage,
    [CreateNotify] = createnotify,
    [ConfigureRequest] = configurerequest,
    [ConfigureNotify] = configurenotify,
    [DestroyNotify] = destroynotify,
    [EnterNotify] = enternotify,
    [Expose] = expose,
    [FocusIn] = focusin,
    [KeyPress] = keypress,
    [MappingNotify] = mappingnotify,
    [MotionNotify] = motionnotify,
    [MapNotify] = mapnotify,
    [MapRequest] = maprequest,
    [PropertyNotify] = propertynotify,
    [UnmapNotify] = unmapnotify,
};

static struct {
    void (*init)();
    void (*destroy)();
} g_modules[] = {
    { ipc_init,         ipc_destroy         },
    { key_init,         key_destroy         },
    { settings_init,    settings_destroy    },
    { clientlist_init,  clientlist_destroy  },
    { layout_init,      layout_destroy      },
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
    if (be->window == g_root && be->subwindow != None) {
        if (mouse_binding_find(be->state, be->button)) {
            mouse_start_drag(event);
        }
    } else {
        if (be->button == Button1 ||
            be->button == Button2 ||
            be->button == Button3) {
            // only change focus on real clicks... not when scrolling
            if (*g_raise_on_click) {
                XRaiseWindow(g_display, be->window);
            }
            focus_window(be->window, false, true);
        }
        // handling of event is finished, now propagate event to window
        XAllowEvents(g_display, ReplayPointer, CurrentTime);
    }
}

void buttonrelease(XEvent* event) {
    HSDebug("name is: ButtonRelease\n");
    mouse_stop_drag();
}
void clientmessage(XEvent* event) {
    HSDebug("name is: ClientMessage\n");
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
    // HSDebug("name is: ConfigureNotify\n");
}
void destroynotify(XEvent* event) {
    // try to unmanage it
    unmanage_client(event->xdestroywindow.window);
}
void enternotify(XEvent* event) {
    HSDebug("name is: EnterNotify, focus = %d\n", event->xcrossing.focus);
    if (*g_focus_follows_mouse && false == event->xcrossing.focus) {
        // sloppy focus
        focus_window(event->xcrossing.window, false, true);
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
            mouse_regrab_all();
        }
    }
}
void motionnotify(XEvent* event) {
    handle_motion_event(event);
}
void mapnotify(XEvent* event) {
    HSDebug("name is: MapNotify\n");
    if (get_client_from_window(event->xmap.window)) {
        // reset focus. so a new window gets the focus if it shall have the
        // input focus
        frame_focus_recursive(g_cur_frame);
    }
}
void maprequest(XEvent* event) {
    HSDebug("name is: MapRequest\n");
    XMapRequestEvent* mapreq = &event->xmaprequest;
    if (is_window_ignored(mapreq->window)
        || is_herbstluft_window(g_display, mapreq->window)) {
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
            ipc_handle_connection(event->xproperty.window, false);
        } else if((client = get_client_from_window(ev->window))) {
            switch (ev->atom) {
                case XA_WM_HINTS:
                    client_update_wm_hints(client);
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
    execute_autostart_file();

    // main loop
    XEvent event;
    while (!g_aboutToQuit) {
        XNextEvent(g_display, &event);
        void (*handler) (XEvent*) = g_default_handler[event.type];
        if (handler != NULL) {
            handler(&event);
        }
    }

    // destroy all subsystems
    for (int i = LENGTH(g_modules); i --> 0;) {
        g_modules[i].destroy();
    }
    XCloseDisplay(g_display);
    // check if wie shall restart an other window manager
    if (g_exec_before_quit) {
        if (g_exec_args) {
            // do actual exec
            execvp(g_exec_args[0], g_exec_args);
            fprintf(stderr, "herbstluftwm: execvp \"%s\"", g_exec_args[0]);
            perror(" failed");
        }
        // on failure or if no other wm given, then fall back
        execvp(argv[0], argv);
        fprintf(stderr, "herbstluftwm: execvp \"%s\"", argv[1]);
        perror(" failed");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}



