#include "mousemanager.h"

#include <X11/cursorfont.h>

#include "globals.h"
#include "mouse.h"

extern Cursor g_cursor;

MouseManager::MouseManager() {
    /* set cursor theme */
    g_cursor = XCreateFontCursor(g_display, XC_left_ptr);
    XDefineCursor(g_display, g_root, g_cursor);
}

MouseManager::~MouseManager() {
    mouse_unbind_all();
    XFreeCursor(g_display, g_cursor);
}
