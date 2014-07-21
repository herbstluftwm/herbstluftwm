
#ifndef __HERBSTLUFT_CLIENT_UTILS_H_
#define __HERBSTLUFT_CLIENT_UTILS_H_

#include <glib.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

GString* window_property_to_g_string(Display* dpy, Window window, Atom atom);
char** argv_duplicate(int argc, char** argv);
void argv_free(int argc, char** argv);
void die(const char *errstr, ...);


#endif
