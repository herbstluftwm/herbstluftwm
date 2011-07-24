
#ifndef __HERBSTLUFT_GLOBALS_H_
#define __HERBSTLUFT_GLOBALS_H_

#include <stdbool.h>
#include <X11/Xlib.h>

#define HERBSTLUFT_VERSION "0.1-GIT, build on "__DATE__
#define HERBSTLUFT_AUTOSTART "herbstluftrc"

#define HERBST_FRAME_CLASS "_HERBST_FRAME"

// connection to x-server
Display*    g_display;
int         g_screen;
Window      g_root;
// some settings/info
bool        g_aboutToQuit;


#endif



