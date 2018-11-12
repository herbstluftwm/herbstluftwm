
#include "client-utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdarg.h>

// inspired by dwm's gettextprop()
char* read_window_property(Display* dpy, Window window, Atom atom) {
    char* result = NULL;
    char** list = NULL;
    int n = 0;
    XTextProperty prop;

    if (0 == XGetTextProperty(dpy, window, &prop, atom)) {
        return NULL;
    }
    // convert text property to a gstring
    if (prop.encoding == XA_STRING
        || prop.encoding == XInternAtom(dpy, "UTF8_STRING", False)) {
        result = strdup((char*)prop.value);
    } else {
        if (XmbTextPropertyToTextList(dpy, &prop, &list, &n) >= Success
            && n > 0 && *list)
        {
            result = strdup((char*)*list);
            XFreeStringList(list);
        }
    }
    XFree(prop.value);
    return result;
}

// duplicates an argument-vector
char** argv_duplicate(int argc, char** argv) {
    if (argc <= 0) return NULL;
    char** new_argv = malloc(sizeof(char*) * argc);
    if (!new_argv) {
        fprintf(stderr, "cannot malloc - there is no memory available\n");
        exit(EXIT_FAILURE);
    }
    int i;
    for (i = 0; i < argc; i++) {
        new_argv[i] = strdup(argv[i]);
    }
    return new_argv;
}

// frees all entries in argument-vector and then the vector itself
void argv_free(int argc, char** argv) {
    if (argc <= 0) return;
    int i;
    for (i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
}
