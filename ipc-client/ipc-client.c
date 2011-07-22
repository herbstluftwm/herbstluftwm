

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
Window root;
Window win;

int send_command(int argc, char* argv[]) {
    XEvent message;
    message.xclient.type = ClientMessage;
    message.xclient.window = root; // send to root window
    message.xclient.format = 8; // interpret data as chars
    strcpy(message.xclient.data.b, "HOHOHO"); // some message
    XSendEvent(dpy, root, True, 0, &message);
    XSendEvent(dpy, root, True, 0, &message);
    printf("sent message\n");
    XFlush(dpy);
    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return EXIT_FAILURE;
    }
    Window root = DefaultRootWindow(dpy);
    Window win = XCreateSimpleWindow(dpy, root, 42, 42, 42, 42, 0, 0, 0);
    send_command(argc-1, argv+1);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return EXIT_SUCCESS;
}




