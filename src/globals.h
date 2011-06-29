
#ifndef __HERBSTLUFT_GLOBALS_H_
#define __HERBSTLUFT_GLOBALS_H_

#include <stdbool.h>
#include <X11/Xlib.h>

// connection to x-server
Display*    g_display;
int         g_screen;
Window      g_root;
// some settings/info
bool        g_aboutToQuit;


#endif



