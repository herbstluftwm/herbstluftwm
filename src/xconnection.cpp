#include "xconnection.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <climits>
#include <iostream>

#include "globals.h"

using std::endl;
using std::make_pair;
using std::pair;
using std::string;
using std::vector;

XConnection::XConnection(Display* disp)
    : m_display(disp) {
    m_screen = DefaultScreen(m_display);
    m_screen_width = DisplayWidth(m_display, m_screen);
    m_screen_height = DisplayHeight(m_display, m_screen);
    m_root = RootWindow(m_display, m_screen);
    utf8StringAtom_ = XInternAtom(m_display, "UTF8_STRING", False);
}

XConnection::~XConnection() {
    HSDebug("Closing display\n");
    XCloseDisplay(m_display);
}

XConnection* XConnection::connect(string display_name) {
    const char* display_str = (display_name != "") ? display_name.c_str() : nullptr;
    Display* d = XOpenDisplay(display_str);
    if (d == nullptr) {
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
        HSDebug("Warning: ignoring X_BadDrawable\n");
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


string XConnection::atomName(Atom atomIdentifier) {
    char* name = XGetAtomName(m_display, atomIdentifier);
    string res = name;
    XFree(name);
    return res;
}

//! The pid of a window or -1 if the pid is not set
int XConnection::windowPid(Window window) {
    // TODO: move to Ewmh
    auto res = getWindowPropertyCardinal(window, atom("_NET_WM_PID"));
    if (!res.has_value() || res.value().size() == 0) {
        return -1;
    } else {
        return res.value()[0];
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

//! from https://stackoverflow.com/a/39884120/4400896
static std::string iso_8859_1_to_utf8(const char* source) {
    string strOut;
    for (int i = 0; source[i] != '\0'; i++) {
        uint8_t ch = source[i];
        if (ch < 0x80) {
            strOut.push_back(ch);
        }
        else {
            strOut.push_back(0xc0 | ch >> 6);
            strOut.push_back(0x80 | (ch & 0x3f));
        }
    }
    return strOut;
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
    if (prop.encoding == XA_STRING) {
        // a XA_STRING is always encoded in ISO 8859-1
        result = iso_8859_1_to_utf8(reinterpret_cast<char *>(prop.value));
    } else if (prop.encoding == utf8StringAtom_) {
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

//! implement XChangeProperty for type=ATOM('UTF8_STRING')
void XConnection::setPropertyString(Window w, Atom property, string value) {
    // according to the XChangeProperty-specification:
    // if format = 8, then the data must be a char array.
    XChangeProperty(m_display, w, property,
        utf8StringAtom_, 8, PropModeReplace,
        (unsigned char*)value.c_str(), value.size());
}

//! implement XSetTextProperty for an array of utf8-strings
void XConnection::setPropertyString(Window w, Atom property, const vector<string>& value)
{
    vector<const char*> value_c_str;
    value_c_str.reserve(value.size());
    for (const auto& s : value) {
        value_c_str.push_back(s.c_str());
    }
    XTextProperty text_prop;
    Xutf8TextListToTextProperty(
        m_display, (char**) value_c_str.data(), value_c_str.size(),
        XUTF8StringStyle, &text_prop);
    XSetTextProperty(m_display, w, &text_prop, property);
    XFree(text_prop.value);
}

//! implement XChangeProperty for type=XA_WINDOW
void XConnection::setPropertyWindow(Window w, Atom property, const vector<Window>& value) {
    // according to the XChangeProperty-specification:
    // if format = 32, then the data must be a long array.
    XChangeProperty(m_display, w, property,
        XA_WINDOW, 32, PropModeReplace,
        (unsigned char*)(value.data()), value.size());
}

//! implement XChangeProperty for type=XA_CARDINAL
void XConnection::setPropertyCardinal(Window w, Atom property, const vector<long>& value) {
    // according to the XChangeProperty-specification:
    // if format = 32, then the data must be a long array.
    XChangeProperty(m_display, w, property,
        XA_CARDINAL, 32, PropModeReplace,
        (unsigned char*)(value.data()), value.size());
}

/** a sincere wrapper around XGetWindowProperty():
 * get a window property of format 32. If the property does not exist
 * or is not of format 32, the return type is None (and the vector is empty).
 * otherwise the content of the property together with its type is returned.
 */
template<typename T> pair<Atom,vector<T>>
    getWindowProperty32(Display* display, Window window, Atom property)
{
    Atom actual_type;
    int format;
    unsigned long bytes_left;
    unsigned long* items_return;
    unsigned long count;
    int status = XGetWindowProperty(display, window,
            property, 0, ULONG_MAX, False, AnyPropertyType,
            &actual_type, &format, &count, &bytes_left,
            (unsigned char**)&items_return);
    if (Success != status || actual_type == None || format == 0) {
        return make_pair(None, vector<T>());
    }
    if (format != 32) {
        // if the property could be read, but is of the wrong format
        XFree(items_return);
        return make_pair(None, vector<T>());
    }
    vector<T> result;
    result.reserve(count);
    for (int i = 0; i < count; i++) {
        result.push_back(items_return[i]);
    }
    XFree(items_return);
    return make_pair(actual_type, result);
}

std::experimental::optional<vector<long>>
    XConnection::getWindowPropertyCardinal(Window window, Atom property)
{
    auto res = getWindowProperty32<long>(m_display, window, property);
    if (res.first != XA_CARDINAL) {
        return {};
    }
    return res.second;
}

std::experimental::optional<vector<Atom>>
    XConnection::getWindowPropertyAtom(Window window, Atom property)
{
    auto res = getWindowProperty32<Atom>(m_display, window, property);
    if (res.first != XA_ATOM) {
        return {};
    }
    return res.second;
}


std::experimental::optional<vector<Window>>
    XConnection::getWindowPropertyWindow(Window window, Atom property)
{
    auto res = getWindowProperty32<Window>(m_display, window, property);
    if (res.first != XA_WINDOW) {
        return {};
    }
    return res.second;
}

//! query all children of the given window via XQueryTree()
vector<Window> XConnection::queryTree(Window window) {
    Window root, parent, *children = nullptr;
    unsigned int count = 0;
    Status status = XQueryTree(m_display, window,
                               &root, &parent, &children, &count);
    if (status == 0) {
        return {};
    }
    vector<Window> result;
    result.reserve(count);
    for (unsigned int i = 0; i < count; i++) {
        result.push_back(children[i]);
    }
    XFree(children);
    return result;
}
