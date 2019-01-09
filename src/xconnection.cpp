#include "xconnection.h"
#include "globals.h"

#include <X11/Xlib.h>
#include <X11/Xproto.h>

#include <iostream>

XConnection::XConnection(Display* disp)
    : m_display(disp) {
    m_screen = DefaultScreen(m_display);
    m_screen_width = DisplayWidth(m_display, m_screen);
    m_screen_height = DisplayHeight(m_display, m_screen);
    m_root = RootWindow(m_display, m_screen);
}
XConnection::~XConnection() {
    HSDebug("Closing display\n");
    XCloseDisplay(m_display);
}

XConnection* XConnection::connect(std::string display_name) {
    char* display_str = (display_name != "") ? (char*)display_name.c_str() : nullptr;
    Display* d = XOpenDisplay(display_str);
    if (d == NULL) {
        std::cerr << "herbstluftwm: XOpenDisplay() failed" << std::endl;
        exit(EXIT_FAILURE);
    }
    return new XConnection(d);
}

static bool g_other_wm_running = false;

// from dwm.c
/* Startup Error handler to check if another window manager
 * is already running. */
static int xerrorstart(Display *dpy, XErrorEvent *ee) {
    g_other_wm_running = true;
    return -1;
}

static int (*g_xerrorxlib)(Display *, XErrorEvent *);

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


// from dwm.c
bool XConnection::checkotherwm() {
    g_other_wm_running = False;
    g_xerrorxlib = XSetErrorHandler(xerrorstart);
    /* this causes an error if some other window manager is running */
    XSelectInput(m_display, DefaultRootWindow(m_display), SubstructureRedirectMask);
    XSync(m_display, False);
    if(g_other_wm_running) {
        return true;
    } else {
        XSetErrorHandler(xerror);
        XSync(g_display, False);
        return false;
    }
}

