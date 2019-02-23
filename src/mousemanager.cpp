#include "mousemanager.h"

#include <X11/cursorfont.h>

#include "globals.h"
#include "mouse.h"

MouseManager::MouseManager() {
    /* set cursor theme */
    cursor = XCreateFontCursor(g_display, XC_left_ptr);
    XDefineCursor(g_display, g_root, cursor);
}

MouseManager::~MouseManager() {
    XFreeCursor(g_display, cursor);
}
