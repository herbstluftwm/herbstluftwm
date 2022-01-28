#include "xconnection.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <fcntl.h>
#include <unistd.h>
#include <climits>
#include <cstring>
#include <iostream>

#include "globals.h"

using std::endl;
using std::make_pair;
using std::pair;
using std::string;
using std::vector;

bool XConnection::exitOnError_ = false;
XConnection* XConnection::s_connection = nullptr;

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
    visual_ = DefaultVisual(m_display, m_screen);
    depth_ = DefaultDepth(m_display, m_screen);
    colormap_ = DefaultColormap(m_display, m_screen);
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
    // set the connection to 'close on exec' such that child
    // processes do not inherit the X connection.
    // if F_GETFD fails (returning -1), then just take '0'
    int flags = std::max(0, fcntl(ConnectionNumber(d), F_GETFD));
    if (fcntl(ConnectionNumber(d), F_SETFD, flags | FD_CLOEXEC) == -1) {
        std::cerr << "herbstluftwm: setting CLOEXEC for the X11 connection socket failed." << endl;
        exit(EXIT_FAILURE);
    }
    s_connection = new XConnection(d);
    return s_connection;
}

/**
 * @brief convert the given color via the given color map
 * or via the default colormap is none is given
 * @param maybeColormap is a colormap or 0
 * @param color
 * @return
 */
unsigned long XConnection::allocColor(Colormap maybeColormap, const Color& color)
{
    XColor xcol = color.toXColor();
    if (maybeColormap) {
        /* get pixel value back appropriate for client */
        /* this possibly adjusts xcol */
        XAllocColor(display(), maybeColormap, &xcol);
    }
    // explicitly set the alpha-byte to the one from the color
    if (usesTransparency() && compositorRunning_ && color.alpha_ != 0xffu) {
        auto scaleValueByAlpha =
                [](unsigned short colorValue, unsigned short alpha) -> unsigned long  {
            // color value is between 0 and 0xffff
            // alpha value is between 0 and 0xff
            // return the multiplied color value between 0 and 0xff
            return (static_cast<unsigned long>(colorValue)
                    * static_cast<unsigned long>(alpha))
                    / 0xffffu;
        };
        unsigned long pixelPremultiplied = 0;
        pixelPremultiplied = color.alpha_;
        pixelPremultiplied <<= 8;
        pixelPremultiplied |= scaleValueByAlpha(xcol.red, color.alpha_);
        pixelPremultiplied <<= 8;
        pixelPremultiplied |= scaleValueByAlpha(xcol.green, color.alpha_);
        pixelPremultiplied <<= 8;
        pixelPremultiplied |= scaleValueByAlpha(xcol.blue, color.alpha_);
        return pixelPremultiplied;
    } else {
        // set alpha-channel to non-opaque
        return xcol.pixel | (0xffu << 24);
    }
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


/**
 * @brief Check whether another window manager listens
 * for events on the root window.
 * @return
 */
bool XConnection::otherWmListensRoot() {
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

void XConnection::tryInitTransparency()
{
    XVisualInfo* infos;
    XRenderPictFormat* fmt;
    int nitems;
    int i;

    XVisualInfo tpl = {
        nullptr,
        0,
        m_screen, // screen
        32, // depth
        TrueColor, // class
        // remaining unused members:
        0, 0, 0, 0, 0,
    };
    long masks = VisualScreenMask | VisualDepthMask | VisualClassMask;

    infos = XGetVisualInfo(m_display, masks, &tpl, &nitems);
    for(i = 0; i < nitems; i ++) {
        fmt = XRenderFindVisualFormat(m_display, infos[i].visual);
        if (fmt->type == PictTypeDirect && fmt->direct.alphaMask) {
            visual_ = infos[i].visual;
            depth_ = infos[i].depth;
            colormap_ = XCreateColormap(m_display, m_root, visual_, AllocNone);
            usesTransparency_ = true;
            break;
        }
    }

    XFree(infos);
}

/**
 * @brief tell xconnection whether a compositor (aka compositing manager)
 * is running at the momenet. this affects the color computation.
 * @param running
 */
void XConnection::setCompositorRunning(bool running)
{
    HSDebug("compositing manager = %d\n", running);
    compositorRunning_ = running;
}

Rectangle XConnection::windowSize(Window window) {
    unsigned int border = 0, depth = 0;
    int x = 0, y = 0;
    unsigned int w = 0, h = 0;
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

//! The pgid of a window or -1 if the pid is not set
int XConnection::windowPgid(Window window) {
    auto pid = windowPid(window);
    if (pid == -1) {
        return -1;
    }
    return getpgid(pid);
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
    std::experimental::optional<vector<string>> elements
            = getWindowPropertyTextList(window, atom);
    if (!elements.has_value()) {
        return {};
    }
    if (elements.value().empty()) {
        return {};
    }
    return elements.value()[0];
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
    int status = Xutf8TextListToTextProperty(
                    m_display, (char**) value_c_str.data(), value_c_str.size(),
                    XUTF8StringStyle, &text_prop);
    if (status != Success) {
        // maybe no locale support.
        status = XmbTextListToTextProperty(
                    m_display, (char**) value_c_str.data(), value_c_str.size(),
                    XStdICCTextStyle, &text_prop);
    }
    if (status != Success) {
        HSDebug("Can not create text list\n");
        return;
    }
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

/**
 * @brief wrapper around XDeleteProperty
 * @param w
 * @param property
 */
void XConnection::deleteProperty(Window w, Atom property)
{
    XDeleteProperty(m_display, w, property);
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
    Atom prop_type;
    int format;
    unsigned long bytes_left;
    unsigned char* items_return;
    unsigned long count;
    int status = XGetWindowProperty(m_display, window,
            property, 0, ULONG_MAX, False, AnyPropertyType,
            &prop_type, &format, &count, &bytes_left,
            &items_return);
    if (Success != status || prop_type == None || format == 0) {
        return {};
    }
    if (format != 8) {
        fprintf(stderr, "herbstluftwm: error: can not parse the"
                        " atom \'%s\' of window 0x%lx: expected format=8 but got"
                        " format=%d\n",
                        atomName(property).c_str(), window, format);
        XFree(items_return);
        return {};
    }
    unsigned long offset = 0;
    vector<string> arguments;
    // the trailing 0 at items_return[count] might be crucial:
    // if the string list ends with the empty string, then we
    // need to access items_return[count].
    while (offset <= count) {
        unsigned char* textChunk = items_return + offset;
        // let us hope that items_return is properly null-byte terminated.
        unsigned long textChunkLen = strlen(reinterpret_cast<char*>(textChunk));
        // copy into a new string object and convert to utf8 if necessary:
        if (prop_type == XA_STRING) {
            // a XA_STRING is always encoded in ISO 8859-1
            arguments.push_back(iso_8859_1_to_utf8(reinterpret_cast<char*>(textChunk)));
        } else if (prop_type == utf8StringAtom_) {
            arguments.push_back(reinterpret_cast<char*>(textChunk));
        } else {
            // try to convert via XmbTextPropertyToTextList, just like
            // xprop does:
            XTextProperty textprop;
            textprop.encoding = prop_type;
            textprop.format = 8;
            textprop.nitems = textChunkLen + 1;
            textprop.value = textChunk;
            int n = 0;
            char** list = nullptr;
            if (XmbTextPropertyToTextList(m_display, &textprop, &list, &n) == Success
                && n > 0 && *list)
            {
                arguments.push_back(*list);
                XFreeStringList(list);
            } else {
                fprintf(stderr, "herbstluftwm: error: can not parse the"
                                " atom \'%s\' of window 0x%lx: unknown text format \'%s\'\n",
                                atomName(property).c_str(),
                                window,
                                atomName(prop_type).c_str());
                XFree(items_return);
                return {};
            }
        }
        // skip the string, and skip the null-byte
        offset += textChunkLen + 1;
    }
    XFree(items_return);
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

#define DetailCodeAndString(C)  { C, #C }
/**
 * @brief print the name of the 'detail' of a XFocusedChangedEvent
 * @param the 'detail' of a XFocusedChangedEvent
 * @return
 */
const char* XConnection::focusChangedDetailToString(int focusedChangedEventDetail)
{
    vector<pair<int, const char*>> table = {
        DetailCodeAndString(NotifyAncestor),
        DetailCodeAndString(NotifyVirtual),
        DetailCodeAndString(NotifyInferior),
        DetailCodeAndString(NotifyNonlinear),
        DetailCodeAndString(NotifyNonlinearVirtual),
        DetailCodeAndString(NotifyPointer),
        DetailCodeAndString(NotifyPointerRoot),
        DetailCodeAndString(NotifyDetailNone),
    };
    for (auto& e : table) {
        if (e.first == focusedChangedEventDetail) {
            return e.second;
        }
    }
    return nullptr;
}

void XConnection::setWindowUrgencyHint(Window window, bool urgent)
{
    XWMHints* wmh;
    if (!(wmh = XGetWMHints(m_display, window))) {
        // just allocate new wm hints for the case the window
        // did not have wm hints set before.
        // here, we ignore what happens on insufficient memory
        wmh = XAllocWMHints();
    }
    bool currentState = (wmh->flags & XUrgencyHint) != 0;
    if (currentState != urgent) {
        if (urgent) {
            wmh->flags |= XUrgencyHint;
        } else {
            wmh->flags &= ~XUrgencyHint;
        }
        XSetWMHints(m_display, window, wmh);
    }
    XFree(wmh);
}
