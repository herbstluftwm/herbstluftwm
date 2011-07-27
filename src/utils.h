
#ifndef __HERBST_UTILS_H_
#define __HERBST_UTILS_H_

#include <glib.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#define LENGTH(X) (sizeof(X)/sizeof(*X))

/// print a printf-like message to stderr and exit
void die(const char *errstr, ...);

// get X11 color from color string
unsigned long getcolor(const char *colstr);

#define ATOM(A) XInternAtom(g_display, (A), False)

GString* window_property_to_g_string(Display* dpy, Window window, Atom atom);
GString* window_class_to_g_string(Display* dpy, Window window);

bool is_herbstluft_window(Display* dpy, Window window);

bool is_window_mapable(Display* dpy, Window window);

// duplicates an argument-vector
char** argv_duplicate(int argc, char** argv);
// frees all entrys in argument-vector and then the vector itself
void argv_free(int argc, char** argv);

#endif


