#ifndef __HLWM_XCONNECTION_H_
#define __HLWM_XCONNECTION_H_

#include <X11/X.h>
#include <X11/Xlib.h>
#include <string>

#include "optional.h"
#include "x11-types.h"

class XConnection {
private:
    XConnection(Display* disp);
public:
    ~XConnection();
    static XConnection* connect(std::string display_name = {});
    Display* display() { return m_display; }
    int screen() { return m_screen; }
    Window root() { return m_root; }
    Window screenWidth() { return m_screen_width; }
    Window screenHeight() { return m_screen_height; }
    bool checkotherwm(); // return whether another WM is running

    // utility functions
    Rectangle windowSize(Window window);
    int windowPid(Window window);
    Atom atom(const char* atom_name);
    std::pair<std::string, std::string> getClassHint(Window win);
    std::string getInstance(Window win) { return getClassHint(win).first; };
    std::string getClass(Window win) { return getClassHint(win).second; };
    std::experimental::optional<std::string> getWindowProperty(Window window, Atom atom);
    void setPropertyWindow(Window w, Atom property, const std::vector<Window>& value);
    void setPropertyCardinal(Window w, Atom property, const std::vector<long>& value);
private:
    Display* m_display;
    int      m_screen;
    Window   m_root;
    int      m_screen_width;
    int      m_screen_height;
};

#endif
