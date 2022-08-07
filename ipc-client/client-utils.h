#ifndef __HERBSTLUFT_CLIENT_UTILS_H_
#define __HERBSTLUFT_CLIENT_UTILS_H_

#include <stdio.h>
#include <X11/X.h>
#include <X11/Xlib.h>

// return a window property or NULL on error
char* read_window_property(Display* dpy, Window window, Atom atom);
char** argv_duplicate(int argc, char** argv);
void argv_free(int argc, char** argv);

/**
 * @brief read until a null byte in a stream, and return the allocated string.
 * if no terminating null byte could be read, then this returns a null pointer.
 * the stream needs to be opened in binary mode.
 * @param the stream to read from
 * @return the string before the null byte; the caller has to free it.
 */
char* read_until_null_byte(int fd);

struct ArgList_ {
    // data[null_index] is always in range and contains NULL
    char** data;
    size_t null_index;
    size_t alloc_length;
};
typedef struct ArgList_ ArgList;
ArgList* arglist_new();
/**
 * @brief push a token to the argument list, and also pass
 * the token's ownership to the ArgList.
 * @param argv
 * @param token
 */
void arglist_push_with_ownership(ArgList* argv, char* token);
void arglist_free(ArgList* argv);

#endif
