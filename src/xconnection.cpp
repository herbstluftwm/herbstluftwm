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

bool XConnection::exitOnError_ = false;

void XConnection::setExitOnError(bool exitOnError)
{
    exitOnError_ = exitOnError;
}

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
    const char* display_str = (!display_name.empty()) ? display_name.c_str() : nullptr;
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
static int xerrorstart(Display*, XErrorEvent*) {
    g_other_wm_running = true;
    return -1;
}

static int (*g_xerrorxlib)(Display *, XErrorEvent *);

// from dwm.c
/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's).  Other types of errors call Xlibs
 * default error handler, which may call exit.  */
int XConnection::xerror(Display *dpy, XErrorEvent *ee) {
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
    char errorCodeString[100] = "unknown";
    XGetErrorText(dpy, ee->error_code, errorCodeString, 100);
    const char* requestCodeString = XConnection::requestCodeToString(ee->request_code);
    if (!requestCodeString) {
        requestCodeString = "unknown";
    }
    fprintf(stderr, "herbstluftwm: fatal error\n"
                    "   resource id: 0x%lx\n"
                    "   request code: %d \"%s\" (minor code: %d)\n"
                    "   error code: %d \"%s\"\n"
                    "   serial number: %ld\n",
            ee->resourceid,
            ee->request_code,
            requestCodeString,
            ee->minor_code,
            ee->error_code,
            errorCodeString,
            ee->serial);
    if (ee->error_code == BadDrawable) {
        HSDebug("Warning: ignoring X_BadDrawable\n");
        return 0;
    }
    if (exitOnError_) {
        return g_xerrorxlib(dpy, ee); // may call exit()
    }
    // otherwise, just ignore this error and try to proceed.
    return 0;
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
        XSetErrorHandler(XConnection::xerror);
        XSync(m_display, False);
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
    if (!res.has_value() || res.value().empty()) {
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
    if (hint.res_name) {
        XFree(hint.res_name);
    }
    if (hint.res_class) {
        XFree(hint.res_class);
    }
    return result;
}

//! from https://stackoverflow.com/a/39884120/4400896
static string iso_8859_1_to_utf8(const char* source) {
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

std::experimental::optional<Window> XConnection::getTransientForHint(Window win)
{
    Window master;
    if (XGetTransientForHint(m_display, win, &master) != 0) {
        return master;
    }
    return {};
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
    long* items_return;
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
    for (size_t i = 0; i < count; i++) {
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

std::experimental::optional<vector<string>>
    XConnection::getWindowPropertyTextList(Window window, Atom property)
{
    XTextProperty text_prop;
    if (!XGetTextProperty(m_display, window, &text_prop, property)) {
        return {};
    }
    char** list_return;
    int count;
    if (Success != Xutf8TextPropertyToTextList(m_display, &text_prop, &list_return, &count)) {
        XFree(text_prop.value);
        return {};
    }
    vector<string> arguments;
    for (int i = 0; i < count; i++) {
        arguments.push_back(list_return[i]);
    }
    XFreeStringList(list_return);
    XFree(text_prop.value);
    return { arguments };
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

#define RequestCodeAndString(C)  { C, #C }
const char* XConnection::requestCodeToString(int requestCode)
{
    // all request codes from X11/Xproto.h
    vector<pair<int, const char*>> requestCodeTable = {
        RequestCodeAndString(X_CreateWindow                 ),
        RequestCodeAndString(X_ChangeWindowAttributes       ),
        RequestCodeAndString(X_GetWindowAttributes          ),
        RequestCodeAndString(X_DestroyWindow                ),
        RequestCodeAndString(X_DestroySubwindows            ),
        RequestCodeAndString(X_ChangeSaveSet                ),
        RequestCodeAndString(X_ReparentWindow               ),
        RequestCodeAndString(X_MapWindow                    ),
        RequestCodeAndString(X_MapSubwindows                ),
        RequestCodeAndString(X_UnmapWindow                  ),
        RequestCodeAndString(X_UnmapSubwindows              ),
        RequestCodeAndString(X_ConfigureWindow              ),
        RequestCodeAndString(X_CirculateWindow              ),
        RequestCodeAndString(X_GetGeometry                  ),
        RequestCodeAndString(X_QueryTree                    ),
        RequestCodeAndString(X_InternAtom                   ),
        RequestCodeAndString(X_GetAtomName                  ),
        RequestCodeAndString(X_ChangeProperty               ),
        RequestCodeAndString(X_DeleteProperty               ),
        RequestCodeAndString(X_GetProperty                  ),
        RequestCodeAndString(X_ListProperties               ),
        RequestCodeAndString(X_SetSelectionOwner            ),
        RequestCodeAndString(X_GetSelectionOwner            ),
        RequestCodeAndString(X_ConvertSelection             ),
        RequestCodeAndString(X_SendEvent                    ),
        RequestCodeAndString(X_GrabPointer                  ),
        RequestCodeAndString(X_UngrabPointer                ),
        RequestCodeAndString(X_GrabButton                   ),
        RequestCodeAndString(X_UngrabButton                 ),
        RequestCodeAndString(X_ChangeActivePointerGrab      ),
        RequestCodeAndString(X_GrabKeyboard                 ),
        RequestCodeAndString(X_UngrabKeyboard               ),
        RequestCodeAndString(X_GrabKey                      ),
        RequestCodeAndString(X_UngrabKey                    ),
        RequestCodeAndString(X_AllowEvents                  ),
        RequestCodeAndString(X_GrabServer                   ),
        RequestCodeAndString(X_UngrabServer                 ),
        RequestCodeAndString(X_QueryPointer                 ),
        RequestCodeAndString(X_GetMotionEvents              ),
        RequestCodeAndString(X_TranslateCoords              ),
        RequestCodeAndString(X_WarpPointer                  ),
        RequestCodeAndString(X_SetInputFocus                ),
        RequestCodeAndString(X_GetInputFocus                ),
        RequestCodeAndString(X_QueryKeymap                  ),
        RequestCodeAndString(X_OpenFont                     ),
        RequestCodeAndString(X_CloseFont                    ),
        RequestCodeAndString(X_QueryFont                    ),
        RequestCodeAndString(X_QueryTextExtents             ),
        RequestCodeAndString(X_ListFonts                    ),
        RequestCodeAndString(X_ListFontsWithInfo            ),
        RequestCodeAndString(X_SetFontPath                  ),
        RequestCodeAndString(X_GetFontPath                  ),
        RequestCodeAndString(X_CreatePixmap                 ),
        RequestCodeAndString(X_FreePixmap                   ),
        RequestCodeAndString(X_CreateGC                     ),
        RequestCodeAndString(X_ChangeGC                     ),
        RequestCodeAndString(X_CopyGC                       ),
        RequestCodeAndString(X_SetDashes                    ),
        RequestCodeAndString(X_SetClipRectangles            ),
        RequestCodeAndString(X_FreeGC                       ),
        RequestCodeAndString(X_ClearArea                    ),
        RequestCodeAndString(X_CopyArea                     ),
        RequestCodeAndString(X_CopyPlane                    ),
        RequestCodeAndString(X_PolyPoint                    ),
        RequestCodeAndString(X_PolyLine                     ),
        RequestCodeAndString(X_PolySegment                  ),
        RequestCodeAndString(X_PolyRectangle                ),
        RequestCodeAndString(X_PolyArc                      ),
        RequestCodeAndString(X_FillPoly                     ),
        RequestCodeAndString(X_PolyFillRectangle            ),
        RequestCodeAndString(X_PolyFillArc                  ),
        RequestCodeAndString(X_PutImage                     ),
        RequestCodeAndString(X_GetImage                     ),
        RequestCodeAndString(X_PolyText8                    ),
        RequestCodeAndString(X_PolyText16                   ),
        RequestCodeAndString(X_ImageText8                   ),
        RequestCodeAndString(X_ImageText16                  ),
        RequestCodeAndString(X_CreateColormap               ),
        RequestCodeAndString(X_FreeColormap                 ),
        RequestCodeAndString(X_CopyColormapAndFree          ),
        RequestCodeAndString(X_InstallColormap              ),
        RequestCodeAndString(X_UninstallColormap            ),
        RequestCodeAndString(X_ListInstalledColormaps       ),
        RequestCodeAndString(X_AllocColor                   ),
        RequestCodeAndString(X_AllocNamedColor              ),
        RequestCodeAndString(X_AllocColorCells              ),
        RequestCodeAndString(X_AllocColorPlanes             ),
        RequestCodeAndString(X_FreeColors                   ),
        RequestCodeAndString(X_StoreColors                  ),
        RequestCodeAndString(X_StoreNamedColor              ),
        RequestCodeAndString(X_QueryColors                  ),
        RequestCodeAndString(X_LookupColor                  ),
        RequestCodeAndString(X_CreateCursor                 ),
        RequestCodeAndString(X_CreateGlyphCursor            ),
        RequestCodeAndString(X_FreeCursor                   ),
        RequestCodeAndString(X_RecolorCursor                ),
        RequestCodeAndString(X_QueryBestSize                ),
        RequestCodeAndString(X_QueryExtension               ),
        RequestCodeAndString(X_ListExtensions               ),
        RequestCodeAndString(X_ChangeKeyboardMapping        ),
        RequestCodeAndString(X_GetKeyboardMapping           ),
        RequestCodeAndString(X_ChangeKeyboardControl        ),
        RequestCodeAndString(X_GetKeyboardControl           ),
        RequestCodeAndString(X_Bell                         ),
        RequestCodeAndString(X_ChangePointerControl         ),
        RequestCodeAndString(X_GetPointerControl            ),
        RequestCodeAndString(X_SetScreenSaver               ),
        RequestCodeAndString(X_GetScreenSaver               ),
        RequestCodeAndString(X_ChangeHosts                  ),
        RequestCodeAndString(X_ListHosts                    ),
        RequestCodeAndString(X_SetAccessControl             ),
        RequestCodeAndString(X_SetCloseDownMode             ),
        RequestCodeAndString(X_KillClient                   ),
        RequestCodeAndString(X_RotateProperties             ),
        RequestCodeAndString(X_ForceScreenSaver             ),
        RequestCodeAndString(X_SetPointerMapping            ),
        RequestCodeAndString(X_GetPointerMapping            ),
        RequestCodeAndString(X_SetModifierMapping           ),
        RequestCodeAndString(X_GetModifierMapping           ),
        RequestCodeAndString(X_NoOperation                  ),
    };
    for (auto& e : requestCodeTable) {
        if (e.first == requestCode) {
            return e.second;
        }
    }
    return nullptr;
}
