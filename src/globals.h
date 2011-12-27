/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_GLOBALS_H_
#define __HERBSTLUFT_GLOBALS_H_

#include <stdbool.h>
#include <X11/Xlib.h>

#define HERBSTLUFT_AUTOSTART "herbstluftwm/autostart"
#define WINDOW_MANAGER_NAME "herbstluftwm"
#define HERBSTLUFT_VERSION_STRING \
    WINDOW_MANAGER_NAME " " HERBSTLUFT_VERSION " (built on " __DATE__ ")"

#define HERBST_FRAME_CLASS "_HERBST_FRAME"
#define WINDOW_MIN_HEIGHT 32
#define WINDOW_MIN_WIDTH 32

#define ROOT_EVENT_MASK (SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask|EnterWindowMask|LeaveWindowMask|StructureNotifyMask)
//#define CLIENT_EVENT_MASK (PropertyChangeMask | FocusChangeMask | StructureNotifyMask)
#define CLIENT_EVENT_MASK (FocusChangeMask|EnterWindowMask|PropertyChangeMask)

// minimum relative fraction of split frames
#define FRAME_MIN_FRACTION 0.1

#define HERBST_MAX_TREE_HEIGHT 3

// connection to x-server
Display*    g_display;
int         g_screen;
Window      g_root;
int         g_screen_width;
int         g_screen_height;
// some settings/info
bool        g_aboutToQuit;
int         g_verbose;

// bufsize to get some error strings
#define ERROR_STRING_BUF_SIZE 1000
// size for some normal string buffers
#define STRING_BUF_SIZE 1000


#define HSDebug(...) \
    do { \
        if (g_verbose) { \
            fprintf(stderr, "%s: %d: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__); \
        } \
    } while(0);

#endif



