#include "client-utils.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    if (argc <= 0) {
        return NULL;
    }
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
    if (argc <= 0) {
        return;
    }
    int i;
    for (i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
}

// static bool read_char_from_stream(FILE* stream, char* ch) {
//     int c = fgetc(stream);
//     if (c == EOF) {
//         return false;
//     }
//     // re-interpret the 'int' from fgetc() as a signed char:
//     char c_signed = (char) ((c > CHAR_MAX) ? (c - (UCHAR_MAX + 1)) : c);
//     *ch = c_signed;
//     return true;
// }

static bool read_char_from_fd(int fd, char* ch) {
    ssize_t count = read(fd, ch, sizeof(*ch));
    return count != 0;
}

char* read_until_null_byte(int fd) {
    size_t alloclen = 10;
    char* buf = malloc(sizeof(char) * alloclen);
    size_t next_pos = 0; // where to write the next byte to
    while (true) {
        char ch;
        // bool suc = read_char_from_stream(stream, &ch);
        bool suc = read_char_from_fd(fd, &ch);
        if (!suc) {
            free(buf);
            return NULL;
        }
        // make sure that buf[next_pos] is in range
        if (next_pos >= alloclen) {
            alloclen += 10;
            buf = realloc(buf, alloclen);
        }
        buf[next_pos] = ch;
        next_pos++;
        if (ch == '\0') {
            // if this was the terminating null byte,
            // then stop scanning
            break;
        }
    }
    return buf;
}

ArgList* arglist_new() {
    ArgList* argv = malloc(sizeof(ArgList));
    argv->alloc_length = 20;
    argv->data = malloc(sizeof(char*) * argv->alloc_length);
    argv->null_index = 0;
    argv->data[0] = NULL;
    return argv;
}

void arglist_push_with_ownership(ArgList* argv, char* token) {
    if (argv->null_index + 1 >= argv->alloc_length) {
        argv->alloc_length += 10;
        argv->data = realloc(argv->data, sizeof(char*) * argv->alloc_length);
    }
    argv->data[argv->null_index] = token;
    argv->null_index++;
    argv->data[argv->null_index] = NULL;
}

void arglist_free(ArgList* argv) {
    for (size_t i = 0; i < argv->null_index; i++) {
        free(argv->data[i]);
    }
    free(argv->data);
    free(argv);
}
