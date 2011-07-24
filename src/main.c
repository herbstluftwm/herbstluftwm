// herbstluftwm
#include "clientlist.h"
#include "utils.h"
#include "key.h"
#include "globals.h"
#include "ipc-server.h"
#include "ipc-protocol.h"
#include "command.h"
// standard
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
// gui
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

static Bool     g_otherwm;
static int (*g_xerrorxlib)(Display *, XErrorEvent *);

int quit();
int version(int argc, char* argv[], GString** result);
int spawn(int argc, char** argv);

int hi() {
    printf("HIHIHI\n");
    return 0;
}
int ho() {
    printf("HOHOHO\n");
    return 0;
}

CommandBinding g_commands[] = {
    CMD_BIND(quit),
    CMD_BIND(version),
    CMD_BIND(list_commands),
    CMD_BIND(hi),
    CMD_BIND(ho),
    CMD_BIND_NO_OUTPUT(keybind),
    CMD_BIND_NO_OUTPUT(keyunbind),
    CMD_BIND_NO_OUTPUT(spawn),
    {{ NULL }}
};

// core funcitons
int quit() {
    g_aboutToQuit = true;
    return 0;
}

int version(int argc, char* argv[], GString** result) {
    (void) argc;
    (void) argv;
    *result = g_string_assign(*result, HERBSTLUFT_VERSION);
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
        fprintf(stderr, "herbstluft: execvp \"%s\"", argv[0]);
        perror(" failed");
        exit(0);
    }
    return 0;
}

// handle x-events:

void event_on_configure(XEvent event) {
    XConfigureRequestEvent* cre = &event.xconfigurerequest;
    //XMoveResizeWindow(g_display, 5, 3, 160,90);
    XWindowChanges wc;
    wc.x = cre->x;
    wc.y = cre->y;
    wc.width = cre->width;
    wc.height = cre->height;
    wc.border_width = cre->border_width;
    wc.sibling = cre->above;
    wc.stack_mode = cre->detail;
    XConfigureWindow(g_display, cre->window, cre->value_mask, &wc);
    XSync(g_display, False);
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
            manage_client(wins[i]);
        }
        if(wins)
            XFree(wins);
    }
}



int main(int argc, char* argv[]) {
    if(!(g_display = XOpenDisplay(NULL)))
        die("herbstluftwm: cannot open display\n");
    checkotherwm();
    // set some globals
    g_screen = DefaultScreen(g_display);
    g_root = RootWindow(g_display, g_screen);
    //scan();
    // keybinds
    XGrabKey(g_display, XKeysymToKeycode(g_display, XStringToKeysym("F1")),
             Mod1Mask, g_root, True, GrabModeAsync, GrabModeAsync);
    XSelectInput(g_display, g_root, SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask|EnterWindowMask|LeaveWindowMask|StructureNotifyMask);
    ipc_init();
    key_init();
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
            case ConfigureNotify: printf("name is: ConfigureNotify\n"); break;
            case DestroyNotify: printf("name is: DestroyNotify\n");
                // TODO: only try to disconnect, if it _had_ the right window-class?
                ipc_disconnect_client(event.xcreatewindow.window);
                break;
            case EnterNotify: printf("name is: EnterNotify\n"); break;
            case Expose: printf("name is: Expose\n"); break;
            case FocusIn: printf("name is: FocusIn\n"); break;
            case KeyPress: printf("name is: KeyPress\n");
                handle_key_press(&event);
                break;
            case MappingNotify: printf("name is: MappingNotify\n"); break;
            case MapRequest: printf("name is: MapRequest\n");
                XMapRequestEvent* mapreq = &event.xmaprequest;
                XMapWindow(g_display, mapreq->window);
            break;
            case PropertyNotify: printf("name is: PropertyNotify\n"); 
                if (is_ipc_connectable(event.xproperty.window)) {
                    ipc_handle_connection(event.xproperty.window);
                }
                break;
            case UnmapNotify: printf("name is: UnmapNotify\n"); break;
            default:
                printf("got unknown event of type %d\n", event.type);
                break;
        }
    }
    // close all
    //free_clients();
    ipc_destroy();
    key_destroy();
    XCloseDisplay(g_display);
    return EXIT_SUCCESS;
}



