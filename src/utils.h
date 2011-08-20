/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

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
bool is_window_mapped(Display* dpy, Window window);

// duplicates an argument-vector
char** argv_duplicate(int argc, char** argv);
// frees all entrys in argument-vector and then the vector itself
void argv_free(int argc, char** argv);

XRectangle parse_rectangle(char* string);

// returns the unichar in GSTR at position GSTR
#define UTF8_STRING_AT(GSTR, OFFS) \
    g_utf8_get_char( \
        g_utf8_offset_to_pointer((GSTR), (OFFS))) \

#define RECTANGLE_EQUALS(a, b) (\
        (a).x == (b).x &&   \
        (a).y == (b).y &&   \
        (a).width == (b).width &&   \
        (a).height == (b).height    \
    )

#endif


