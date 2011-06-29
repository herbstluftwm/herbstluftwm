// herbstluftwm
#include "clientlist.h"
#include "utils.h"
#include "globals.h"
// standard
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
// gui
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

static Bool     g_otherwm;
static int (*g_xerrorxlib)(Display *, XErrorEvent *);


// core funcitons
void quit(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    g_aboutToQuit = true;
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
    // main loop
    int offset = 0;
    XEvent event;
    while (!g_aboutToQuit) {
        XNextEvent(g_display, &event);
        printf("got event of type %d\n", event.type);
        switch (event.type) {
            case ButtonPress: printf("name is: ButtonPress\n"); break;
            case ClientMessage: printf("name is: ClientMessage\n"); break;
            case ConfigureRequest: printf("name is: ConfigureRequest\n");
                XConfigureRequestEvent* cre = &event.xconfigurerequest;
                //XMoveResizeWindow(g_display, 5, 3, 160,90);
                XWindowChanges wc;
                wc.x = cre->x + offset;
                wc.y = cre->y + offset;
                offset += 10;
                wc.width = cre->width;
                wc.height = cre->height;
                wc.border_width = cre->border_width;
                wc.sibling = cre->above;
                wc.stack_mode = cre->detail;
                XConfigureWindow(g_display, cre->window, cre->value_mask, &wc);
                XSync(g_display, False);
                break;
            case ConfigureNotify: printf("name is: ConfigureNotify\n"); break;
            case DestroyNotify: printf("name is: DestroyNotify\n"); break;
            case EnterNotify: printf("name is: EnterNotify\n"); break;
            case Expose: printf("name is: Expose\n"); break;
            case FocusIn: printf("name is: FocusIn\n"); break;
            case KeyPress: printf("name is: KeyPress\n"); break;
            case MappingNotify: printf("name is: MappingNotify\n"); break;
            case MapRequest: printf("name is: MapRequest\n");
                XMapRequestEvent* mapreq = &event.xmaprequest;
                XMapWindow(g_display, mapreq->window);
            break;
            case PropertyNotify: printf("name is: PropertyNotify\n"); break;
            case UnmapNotify: printf("name is: UnmapNotify\n"); break;
        }
        if (event.type == KeyPress) {
            quit(0,0);
        }
    }
    // close all
    //free_clients();
    XCloseDisplay(g_display);
    return EXIT_SUCCESS;
}



