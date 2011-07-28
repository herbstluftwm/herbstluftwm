/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "globals.h"
#include "utils.h"
// standard
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// gui
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <glib.h>


/// print a printf-like message to stderr and exit
// from dwm.c
void die(const char *errstr, ...) {
    va_list ap;
    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

// get X11 color from color string
// from dwm.c
unsigned long getcolor(const char *colstr) {
    Colormap cmap = DefaultColormap(g_display, g_screen);
    XColor color;
    if(!XAllocNamedColor(g_display, cmap, colstr, &color, &color))
        die("error, cannot allocate color '%s'\n", colstr);
    return color.pixel;
}

GString* window_property_to_g_string(Display* dpy, Window window, Atom atom) {
    GString* result = g_string_new("");
    long bufsize = 10;
    char *buf;
    Atom type;
    int format;
    unsigned long items, bytes;
    long offset = 0;
    bool parse_error_occured = false;
    do {
        int status = XGetWindowProperty(dpy, window,
            atom, offset, bufsize, False,
            ATOM("UTF8_STRING"), &type, &format,
            &items, &bytes, (unsigned char**)&buf);
        if (status != Success) {
            parse_error_occured = true;
            break; // then stop parsing
        } else {
            result = g_string_append(result, buf);
            offset += bufsize;
            XFree(buf);
        }
        //printf("recieved: \"%s\"\n", result->str);
    } while (bytes > 0);
    //
    if (parse_error_occured) {
        // then just return NULL
        g_string_free(result, true);
        return NULL;
    } else {
        return result;
    }
}

GString* window_class_to_g_string(Display* dpy, Window window) {
    XClassHint hint;
    if (0 == XGetClassHint(dpy, window, &hint)) {
        return g_string_new("");
    }
    GString* string = g_string_new(hint.res_class ? hint.res_class : "");
    if (hint.res_name) XFree(hint.res_name);
    if (hint.res_class) XFree(hint.res_class);
    return string;
}

bool is_herbstluft_window(Display* dpy, Window window) {
    GString* string = window_class_to_g_string(dpy, window);
    bool result;
    result = !strcmp(string->str, HERBST_FRAME_CLASS);
    g_string_free(string, true);
    return result;
}

bool is_window_mapable(Display* dpy, Window window) {
    XWindowAttributes wa;
    XGetWindowAttributes(dpy, window,  &wa);
    return (wa.map_state == IsUnmapped);
}

// duplicates an argument-vector
char** argv_duplicate(int argc, char** argv) {
    char** new_argv = malloc(sizeof(char*) * argc);
    if (!new_argv) {
        die("cannot malloc - there is no memory available\n");
    }
    int i;
    for (i = 0; i < argc; i++) {
        new_argv[i] = g_strdup(argv[i]);
    }
    return new_argv;
}

// frees all entrys in argument-vector and then the vector itself
void argv_free(int argc, char** argv) {
    int i;
    for (i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
}


XRectangle parse_rectangle(char* string) {
    XRectangle rect;
    int x,y;
    unsigned int w, h;
    XParseGeometry(string, &x, &y, &w, &h);
    rect.x = (short int)x;
    rect.y = (short int)y;
    rect.width = (unsigned short int)w;
    rect.height = (unsigned short int)h;
    return rect;
}


