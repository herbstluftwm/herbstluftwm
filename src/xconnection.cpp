#include "xconnection.h"
#include "globals.h"

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>

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

XConnection XConnection::connect(std::string display_name) {
    char* display_str = (display_name != "") ? (char*)display_name.c_str() : NULL;
    Display* d = XOpenDisplay(display_str);
    return XConnection(d);
}

