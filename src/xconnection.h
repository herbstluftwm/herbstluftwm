#ifndef __HLWM_XCONNECTION_H_
#define __HLWM_XCONNECTION_H_

#include <X11/X.h>
#include <X11/Xlib.h>
#include <string>

#include "optional.h"
#include "rectangle.h"

class XConnection {
private:
    XConnection(Display* disp);
    // forbid copy constructor:
    XConnection(const XConnection&) = delete;
public:
    ~XConnection();
    static XConnection* connect(std::string display_name = {});
    static XConnection& get() { return *s_connection; }
    Display* display() { return m_display; }
    int screen() { return m_screen; }
    Window root() { return m_root; }
    Window screenWidth() { return m_screen_width; }
    Window screenHeight() { return m_screen_height; }
    Colormap colormap() { return colormap_; }
    int depth() { return depth_; }
    Visual* visual() { return visual_; }

    bool otherWmListensRoot(); // return whether another WM is running
    void tryInitTransparency();
    bool usesTransparency() { return usesTransparency_; }

    // utility functions
    static const char* requestCodeToString(int requestCode);
    Rectangle windowSize(Window window);
    int windowPid(Window window);
    int windowPgid(Window window);
    Atom atom(const char* atom_name);
    std::string atomName(Atom atomIdentifier);
    std::pair<std::string, std::string> getClassHint(Window win);
    std::string getInstance(Window win) { return getClassHint(win).first; };
    std::string getClass(Window win) { return getClassHint(win).second; };
    std::experimental::optional<std::string> getWindowProperty(Window window, Atom atom);
    std::experimental::optional<std::vector<long>>
        getWindowPropertyCardinal(Window window, Atom property);
    std::experimental::optional<std::vector<Atom>>
        getWindowPropertyAtom(Window window, Atom property);
    std::experimental::optional<std::vector<Window>>
        getWindowPropertyWindow(Window window, Atom property);
    std::experimental::optional<std::vector<std::string>>
        getWindowPropertyTextList(Window window, Atom property);
    void setPropertyString(Window w, Atom property, std::string value);
    void setPropertyString(Window w, Atom property, const std::vector<std::string>& value);
    void setPropertyWindow(Window w, Atom property, const std::vector<Window>& value);
    void setPropertyCardinal(Window w, Atom property, const std::vector<long>& value);
    std::experimental::optional<Window> getTransientForHint(Window win);
    std::vector<Window> queryTree(Window window);
    static void setExitOnError(bool exitOnError);
private:
    static int xerror(Display *dpy, XErrorEvent *ee);
    Display* m_display;
    int      m_screen;
    Window   m_root;
    int      m_screen_width;
    int      m_screen_height;
    Atom     utf8StringAtom_;
    int depth_;
    Visual* visual_;
    Colormap colormap_;
    bool usesTransparency_ = false;
    static bool     exitOnError_; //! exit on any xlib error
    static XConnection* s_connection;
};

#endif
