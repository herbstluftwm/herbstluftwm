#ifndef __HERBSTLUFT_CLIENT_UTILS_H_
#define __HERBSTLUFT_CLIENT_UTILS_H_

#include <X11/X.h>
#include <X11/Xlib.h>

// return a window property or NULL on error
char* read_window_property(Display* dpy, Window window, Atom atom);
char** argv_duplicate(int argc, char** argv);
void argv_free(int argc, char** argv);


#endif
