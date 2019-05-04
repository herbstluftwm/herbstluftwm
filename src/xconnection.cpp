#include "xconnection.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <iostream>

#include "globals.h"

using std::endl;
using std::pair;
using std::string;

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

XConnection* XConnection::connect(string display_name) {
    char* display_str = (display_name != "") ? (char*)display_name.c_str() : nullptr;
    Display* d = XOpenDisplay(display_str);
    if (d == NULL) {
        std::cerr << "herbstluftwm: XOpenDisplay() failed" << endl;
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

Rectangle XConnection::windowSize(Window window) {
    unsigned int border, depth;
    int x, y;
    unsigned int w, h;
    XGetGeometry(m_display, window, &m_root, &x, &y, &w, &h, &border, &depth);
    // treat wanted coordinates as floating coords
    return { x, y, (int)w, (int)h };
}

Atom XConnection::atom(const char* atom_name) {
    return XInternAtom(m_display, atom_name, False);
}

//! The pid of a window or -1 if the pid is not set
int XConnection::windowPid(Window window) {
    Atom type;
    int format;
    unsigned long items, remain;
    int* buf;
    int status = XGetWindowProperty(m_display, window,
        atom("_NET_WM_PID"), 0, 1, False,
        XA_CARDINAL, &type, &format,
        &items, &remain, (unsigned char**)&buf);
    if (items == 1 && format == 32 && remain == 0
        && type == XA_CARDINAL && status == Success) {
        int value = *buf;
        XFree(buf);
        return value;
    } else {
        return -1;
    }
}

//! wrapper around XGetClassHint returning the window's instance and class name
pair<string, string> XConnection::getClassHint(Window window) {
    XClassHint hint;
    if (0 == XGetClassHint(m_display, window, &hint)) {
        return {"", ""};
    }
    pair<string,string> result = {
        hint.res_name ? hint.res_name : "",
        hint.res_class ? hint.res_class : ""
    };
    if (hint.res_name) XFree(hint.res_name);
    if (hint.res_class) XFree(hint.res_class);
    return result;
}

std::experimental::optional<string> XConnection::getWindowProperty(Window window, Atom atom) {
    string result;
    char** list = nullptr;
    int n = 0;
    XTextProperty prop;

    if (0 == XGetTextProperty(m_display, window, &prop, atom)) {
        return std::experimental::optional<string>();
    }
    // convert text property to a gstring
    if (prop.encoding == XA_STRING
        || prop.encoding == XInternAtom(m_display, "UTF8_STRING", False)) {
        result = reinterpret_cast<char *>(prop.value);
    } else {
        if (XmbTextPropertyToTextList(m_display, &prop, &list, &n) >= Success
            && n > 0 && *list)
        {
            result = *list;
            XFreeStringList(list);
        }
    }
    XFree(prop.value);
    return result;
}


//! implement XChangeProperty for type=XA_WINDOW
void XConnection::setPropertyWindow(Window w, Atom property, std::vector<Window> value) {
    // according to the XChangeProperty-specification:
    // if format = 32, then the data must be a long array.
    XChangeProperty(m_display, w, property,
        XA_WINDOW, 32, PropModeReplace,
        (unsigned char*)(value.data()), value.size());
}
