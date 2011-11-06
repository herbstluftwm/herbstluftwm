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
#define SHIFT(ARGC, ARGV) (--(ARGC) && ++(ARGV))

/// print a printf-like message to stderr and exit
void die(const char *errstr, ...);

// get X11 color from color string
unsigned long getcolor(const char *colstr);

#define ATOM(A) XInternAtom(g_display, (A), False)

GString* window_property_to_g_string(Display* dpy, Window window, Atom atom);
GString* window_class_to_g_string(Display* dpy, Window window);
GString* window_instance_to_g_string(Display* dpy, Window window);
int window_pid(Display* dpy, Window window);

bool is_herbstluft_window(Display* dpy, Window window);

bool is_window_mapable(Display* dpy, Window window);
bool is_window_mapped(Display* dpy, Window window);

bool window_has_property(Display* dpy, Window window, char* prop_name);

bool string_to_bool(char* string, bool oldvalue);

char* strlasttoken(char* str, char* delim);

// duplicates an argument-vector
char** argv_duplicate(int argc, char** argv);
// frees all entrys in argument-vector and then the vector itself
void argv_free(int argc, char** argv);

XRectangle parse_rectangle(char* string);

void g_queue_remove_element(GQueue* queue, GList* elem);

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


