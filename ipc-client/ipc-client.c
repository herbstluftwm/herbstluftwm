
#include "../src/ipc-protocol.h"
#include "../src/utils.h"

// standard
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

// gui
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>


int send_command(int argc, char* argv[]);


Display* dpy;
Display* g_display;
Window root;

#define WAIT_IPC_RESPONSE \
    do { \
        XEvent next_event; \
        XNextEvent(dpy, &next_event); \
        if (next_event.type != ClientMessage) { \
            /* discard all other events */ \
            continue; \
        } \
        /* get content */ \
        if (next_event.xclient.format != 8) { \
            /* wrong format */ \
            die("IPC-Response has unknown format\n"); \
        } \
        if (strcmp(HERBST_IPC_SUCCESS, next_event.xclient.data.b)) { \
            /* wrong response */ \
            die("Wrong IPC-Reponse: expected \"%s\" but got \"%s\"\n", \
                HERBST_IPC_SUCCESS, \
                next_event.xclient.data.b); \
        } \
        break; \
    } while (1);

int send_command(int argc, char* argv[]) {
    // create window
    Window win = XCreateSimpleWindow(dpy, root, 42, 42, 42, 42, 0, 0, 0);
    // set wm_class for window
    XClassHint *hint = XAllocClassHint();
    hint->res_name = HERBST_IPC_CLASS;
    hint->res_class = HERBST_IPC_CLASS;
    XSetClassHint(dpy, win, hint);
    XFree(hint);
    // recieve response
    WAIT_IPC_RESPONSE;
    // send argument count
    XChangeProperty(dpy, win, ATOM(HERBST_IPC_ARGC_ATOM),
        XA_ATOM, 32, PropModeReplace, (unsigned char*)&argc, 1);
    WAIT_IPC_RESPONSE;
    // send arguments
    int i;
    for (i = 0; i < argc; i++) {
        XChangeProperty(dpy, win, ATOM(HERBST_IPC_ARGV_ATOM),
            ATOM("UTF8_STRING"), 8, PropModeReplace,
            (unsigned char*)argv[i], strlen(argv[i])+1);
        WAIT_IPC_RESPONSE;
    }
    // wait for output
    WAIT_IPC_RESPONSE;
    // fetch status code
    int command_status = HERBST_UNKNOWN_ERROR;
    int *value;
    Atom type;
    int format;
    unsigned long items, bytes;
    if (Success != XGetWindowProperty(dpy, win,
        ATOM(HERBST_IPC_STATUS_ATOM), 0, 1, False,
        XA_ATOM, &type, &format, &items, &bytes, (unsigned char**)&value)) {
        // if couldnot get window property
        die("couldnot get WindowProperty \"%s\"\n", HERBST_IPC_STATUS_ATOM);
    }
    // on success:
    command_status = *value;
    XFree(value);
    // fetch actual command output
    GString* output = window_property_to_g_string(dpy, win, ATOM(HERBST_IPC_OUTPUT_ATOM));
    if (!output) {
        // if couldnot get window property
        die("couldnot get WindowProperty \"%s\"\n", HERBST_IPC_OUTPUT_ATOM);
    }
    // print output to stdout
    fputs(output->str, stdout);
    // clean all up
    g_string_free(output, true);
    XDestroyWindow(dpy, win);
    return command_status;
}

int main(int argc, char* argv[]) {
    dpy = XOpenDisplay(NULL);
    g_display = dpy;
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return EXIT_FAILURE;
    }
    root = DefaultRootWindow(dpy);
    int command_status = send_command(argc-1, argv+1);
    XCloseDisplay(dpy);
    return command_status;
}




