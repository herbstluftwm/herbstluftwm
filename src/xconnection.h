#ifndef __HLWM_XCONNECTION_H_
#define __HLWM_XCONNECTION_H_

#include <X11/X.h>
#include <X11/Xlib.h>
#include <string>

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
private:
    Display* m_display;
    int      m_screen;
    Window   m_root;
    int      m_screen_width;
    int      m_screen_height;
};

#endif
