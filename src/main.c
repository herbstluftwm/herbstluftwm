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
// standard
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
// gui
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>

static Bool     g_otherwm;
static int (*g_xerrorxlib)(Display *, XErrorEvent *);
static Cursor g_cursor;
static char*    g_autostart_path = NULL; // if not set, then find it in $HOME or $XDG_CONFIG_HOME


int quit();
int reload();
int version(int argc, char* argv[], GString** result);
int print_layout_command(int argc, char** argv, GString** result);
void execute_autostart_file();
int spawn(int argc, char** argv);

CommandBinding g_commands[] = {
    CMD_BIND_NO_OUTPUT(quit),
    CMD_BIND_NO_OUTPUT(reload),
    CMD_BIND(version),
    CMD_BIND(list_commands),
    CMD_BIND(list_monitors),
    CMD_BIND_NO_OUTPUT(keybind),
    CMD_BIND_NO_OUTPUT(hook_emit),
    CMD_BIND_NO_OUTPUT(keyunbind),
    CMD_BIND_NO_OUTPUT(spawn),
    {{ .no_output = frame_current_cycle_selection }, .name = "cycle", .has_output = 0 },
    {{ .no_output = frame_current_cycle_client_layout }, .name = "cycle_layout", .has_output = 0 },
    {{ .no_output = window_close_current }, .name = "close", .has_output = 0 },
    {{ .no_output = frame_split_command }, .name = "split", .has_output = 0 },
    {{ .no_output = frame_focus_command }, .name = "focus", .has_output = 0 },
    {{ .no_output = frame_move_window_command }, .name = "shift", .has_output = 0 },
    {{ .no_output = frame_remove_command }, .name = "remove", .has_output = 0 },
    {{ .no_output = settings_set }, .name = "set", .has_output = 0 },
    {{ .no_output = settings_toggle }, .name = "toggle", .has_output = 0 },
    {{ .no_output = monitor_cycle_command }, .name = "cycle_monitor", .has_output = 0 },
    {{ .standard = settings_get }, .name = "get", .has_output = 1 },
    {{ .no_output = tag_add_command }, .name = "add", .has_output = 0 },
    {{ .no_output = monitor_set_tag_command }, .name = "use", .has_output = 0 },
    {{ .no_output = tag_remove_command }, .name = "merge_tag", .has_output = 0 },
    {{ .no_output = tag_rename_command }, .name = "rename", .has_output = 0 },
    {{ .no_output = tag_move_window_command }, .name = "move", .has_output = 0 },
    {{ .no_output = add_monitor_command }, .name = "add_monitor", .has_output = 0 },
    {{ .no_output = remove_monitor_command }, .name = "remove_monitor", .has_output = 0 },
    {{ .no_output = move_monitor_command }, .name = "move_monitor", .has_output = 0 },
    {{ .standard = print_layout_command }, .name = "layout", .has_output = 1 },
    {{ .standard = complete_command }, .name = "complete", .has_output = 1 },
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
    *result = g_string_assign(*result, HERBSTLUFT_VERSION);
    return 0;
}

int print_layout_command(int argc, char** argv, GString** result) {
    (void) argc;
    (void) argv;
    HSMonitor* m = &g_array_index(g_monitors, HSMonitor, g_cur_monitor);
    HSTag* tag = m->tag;
    if (!tag) {
        return 0;
    }
    print_tag_tree(result);
    return 0;
}

// spawn() heavily inspired by dwm.c
int spawn(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "spawn: to few parameters\n");
        return HERBST_INVALID_ARGUMENT;
    }
    if (fork() == 0) {
        // only look in child
        if (g_display) {
            close(ConnectionNumber(g_display));
        }
        // shift all args in argv by 1 to the front
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
        fprintf(stderr, "herbstluft: execvp \"%s\"", argv[1]);
        perror(" failed");
        exit(0);
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
                g_warning("Willnot parse config file. "
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
        {"autostart", 1, 0, 'c'},
        {0, 0, 0, 0}
    };
    int arg_index = 1; // index of the first-non-option argument
    // parse options
    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "+c:", long_options, &option_index);
        if (c == -1) break;
        switch (c) {
            case 'c':
                g_autostart_path = optarg;
                arg_index++;
                break;
            default:
                fprintf(stderr, "unknown option `%s'\n", argv[arg_index]);
                exit(EXIT_FAILURE);
        }
        arg_index++;
    }
}

int main(int argc, char* argv[]) {
    parse_arguments(argc, argv);
    if(!(g_display = XOpenDisplay(NULL)))
        die("herbstluftwm: cannot open display\n");
    checkotherwm();
    // set some globals
    g_screen = DefaultScreen(g_display);
    g_root = RootWindow(g_display, g_screen);
    // keybinds
    XGrabKey(g_display, XKeysymToKeycode(g_display, XStringToKeysym("F1")),
             Mod1Mask, g_root, True, GrabModeAsync, GrabModeAsync);
    XSelectInput(g_display, g_root, SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask|EnterWindowMask|LeaveWindowMask|StructureNotifyMask);
    ipc_init();
    key_init();
    settings_init();
    clientlist_init();
    layout_init();
    hook_init();
    ensure_monitors_are_available();
    scan();
    all_monitors_apply_layout();
    /* init mouse */
    g_cursor = XCreateFontCursor(g_display, XC_left_ptr);
    XDefineCursor(g_display, g_root, g_cursor);
    execute_autostart_file();
    // main loop
    XEvent event;
    while (!g_aboutToQuit) {
        XNextEvent(g_display, &event);
        switch (event.type) {
            case ButtonPress: printf("name is: ButtonPress\n"); break;
            case ClientMessage: printf("name is: ClientMessage\n"); break;
            case CreateNotify:printf("name is: CreateNotify\n");
                if (is_ipc_connectable(event.xcreatewindow.window)) {
                    ipc_handle_connection(event.xcreatewindow.window);
                }
                break;
            case ConfigureRequest: printf("name is: ConfigureRequest\n");
                event_on_configure(event);
                break;
            case ConfigureNotify: printf("name is: ConfigureNotify\n");
                break;
            case DestroyNotify: printf("name is: DestroyNotify\n");
                // TODO: only try to disconnect, if it _had_ the right window-class?
                ipc_disconnect_client(event.xcreatewindow.window);
                unmanage_client(event.xcreatewindow.window);
                break;
            case EnterNotify: printf("name is: EnterNotify\n"); break;
            case Expose: printf("name is: Expose\n"); break;
            case FocusIn: printf("name is: FocusIn\n"); break;
            case KeyPress: printf("name is: KeyPress\n");
                handle_key_press(&event);
                break;
            case MappingNotify: printf("name is: MappingNotify\n");
                break;
            case MapNotify: printf("name is: MapNotify\n");
                // reset focus.. just to be sure
                frame_focus_recursive(g_cur_frame);
                break;
            case MapRequest: printf("name is: MapRequest\n");
                XMapRequestEvent* mapreq = &event.xmaprequest;
                if (is_window_ignored(mapreq->window)) {
                    // just map the window if it wants that
                    XWindowAttributes wa;
                    if (!XGetWindowAttributes(g_display, mapreq->window, &wa)) {
                        break;
                    }
                    XMapWindow(g_display, mapreq->window);
                } else if (!get_client_from_window(mapreq->window)) {
                    // client should be managed (is not ignored)
                    // but is not managed yet
                    manage_client(mapreq->window);
                    XMapWindow(g_display, mapreq->window);
                }
                // else: ignore all other maprequests from windows
                // that are managed already
            break;
            case PropertyNotify: //printf("name is: PropertyNotify\n"); 
                if (is_ipc_connectable(event.xproperty.window)) {
                    ipc_handle_connection(event.xproperty.window);
                }
                break;
            case UnmapNotify:
                printf("name is: UnmapNotify\n");
                HSMonitor* m2 = &g_array_index(g_monitors, HSMonitor, g_cur_monitor);
                frame_remove_window(m2->tag->frame, event.xunmap.window);
                monitor_apply_layout(m2);
                break;
            default:
                printf("got unknown event of type %d\n", event.type);
                break;
        }
    }
    // close all
    XFreeCursor(g_display, g_cursor);
    hook_destroy();
    layout_destroy();
    ipc_destroy();
    key_destroy();
    settings_destroy();
    clientlist_destroy();
    XCloseDisplay(g_display);
    return EXIT_SUCCESS;
}



