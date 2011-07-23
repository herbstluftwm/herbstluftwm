
#include "globals.h"
#include "utils.h"
// standard
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
// gui
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>


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



