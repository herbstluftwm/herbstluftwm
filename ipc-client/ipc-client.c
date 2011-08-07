/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "../src/ipc-protocol.h"
#include "../src/utils.h"
#include "../src/globals.h"

// standard
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <regex.h>
#include <assert.h>

// gui
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>


int send_command(int argc, char* argv[]);


Display* dpy;
Display* g_display;
Window root;
int g_ensure_newline = 1; // if set, output ends with an newline
int g_wait_for_hook = 0; // if set, donot execute command but wait
int g_hook_count = 1; // count of hooks to wait for, 0 means: forever
regex_t* g_hook_regex = NULL;
int g_hook_regex_count = 0;

void init_hook_regex(int argc, char* argv[]);
void destroy_hook_regex();

static void quit_herbstclient(int signal) {
    // TODO: better solution to quit x connection more softly?
    fprintf(stderr, "interrupted by signal %d\n", signal);
    destroy_hook_regex();
    exit(EXIT_FAILURE);
}

#define WAIT_IPC_RESPONSE \
    do { \
        XEvent next_event; \
        XNextEvent(dpy, &next_event); \
        if (next_event.type != ClientMessage) { \
            /* discard all other events */ \
            continue; \
        } \
        /* get content */ \
        if (next_event.xclient.format != 8) { \
            /* wrong format */ \
            die("IPC-Response has unknown format\n"); \
        } \
        if (strcmp(HERBST_IPC_SUCCESS, next_event.xclient.data.b)) { \
            /* wrong response */ \
            die("Wrong IPC-Reponse: expected \"%s\" but got \"%s\"\n", \
                HERBST_IPC_SUCCESS, \
                next_event.xclient.data.b); \
        } \
        break; \
    } while (1);

int send_command(int argc, char* argv[]) {
    // create window
    Window win = XCreateSimpleWindow(dpy, root, 42, 42, 42, 42, 0, 0, 0);
    // set wm_class for window
    XClassHint *hint = XAllocClassHint();
    hint->res_name = HERBST_IPC_CLASS;
    hint->res_class = HERBST_IPC_CLASS;
    XSetClassHint(dpy, win, hint);
    XFree(hint);
    // recieve response
    WAIT_IPC_RESPONSE;
    // send argument count
    XChangeProperty(dpy, win, ATOM(HERBST_IPC_ARGC_ATOM),
        XA_ATOM, 32, PropModeReplace, (unsigned char*)&argc, 1);
    WAIT_IPC_RESPONSE;
    // send arguments
    int i;
    for (i = 0; i < argc; i++) {
        XChangeProperty(dpy, win, ATOM(HERBST_IPC_ARGV_ATOM),
            ATOM("UTF8_STRING"), 8, PropModeReplace,
            (unsigned char*)argv[i], strlen(argv[i])+1);
        WAIT_IPC_RESPONSE;
    }
    // wait for output
    WAIT_IPC_RESPONSE;
    // fetch status code
    int command_status = HERBST_UNKNOWN_ERROR;
    int *value;
    Atom type;
    int format;
    unsigned long items, bytes;
    if (Success != XGetWindowProperty(dpy, win,
        ATOM(HERBST_IPC_STATUS_ATOM), 0, 1, False,
        XA_ATOM, &type, &format, &items, &bytes, (unsigned char**)&value)) {
        // if couldnot get window property
        die("couldnot get WindowProperty \"%s\"\n", HERBST_IPC_STATUS_ATOM);
    }
    // on success:
    command_status = *value;
    XFree(value);
    // fetch actual command output
    GString* output = window_property_to_g_string(dpy, win, ATOM(HERBST_IPC_OUTPUT_ATOM));
    if (!output) {
        // if couldnot get window property
        die("couldnot get WindowProperty \"%s\"\n", HERBST_IPC_OUTPUT_ATOM);
    }
    // print output to stdout
    fputs(output->str, stdout);
    if (g_ensure_newline) {
        if (output->len > 0 && output->str[output->len - 1] != '\n') {
            fputs("\n", stdout);
        }
    }
    // clean all up
    g_string_free(output, true);
    XDestroyWindow(dpy, win);
    return command_status;
}

void init_hook_regex(int argc, char* argv[]) {
    g_hook_regex = (regex_t*)malloc(sizeof(regex_t)*argc);
    assert(g_hook_regex != NULL);
    int i;
    // create all regexes
    for (i = 0; i < argc; i++) {
        int status = regcomp(g_hook_regex + i, argv[i], REG_NOSUB|REG_EXTENDED);
        if (status != 0) {
            char buf[ERROR_STRING_BUF_SIZE];
            regerror(status, g_hook_regex + i, buf, ERROR_STRING_BUF_SIZE);
            fprintf(stderr, "Cannot parse regex \"%s\": ", argv[i]);
            fprintf(stderr, "%s\n", buf);
            destroy_hook_regex();
            exit(EXIT_FAILURE);
        }
    }
    g_hook_regex_count = argc;
}
void destroy_hook_regex() {
    int i;
    for (i = 0; i < g_hook_regex_count; i++) {
        regfree(g_hook_regex + i);
    }
    free(g_hook_regex);
}

int wait_for_hook(int argc, char* argv[]) {
    // install signals
    signal(SIGTERM, quit_herbstclient);
    signal(SIGINT, quit_herbstclient);
    signal(SIGQUIT, quit_herbstclient);
    init_hook_regex(argc, argv);
    // get window to listen at
    int *value;
    Atom type;
    int format;
    unsigned long items, bytes;
    int status = XGetWindowProperty(dpy, root,
        ATOM(HERBST_HOOK_WIN_ID_ATOM), 0, 1, False,
        XA_ATOM, &type, &format, &items, &bytes, (unsigned char**)&value);
    if (status != Success) {
        fprintf(stderr, "no running herbstluftwm detected\n");
        return EXIT_FAILURE;
    }
    Window win = *value;
    XFree(value);
    // listen on window
    XSelectInput(dpy, win, PropertyChangeMask);
    XEvent next_event;
    while (1) {
        XNextEvent(dpy, &next_event);
        if (next_event.type != PropertyNotify) {
            fprintf(stderr, "Warning: got other event than PropertyNotify\n");
            continue;
        }
        XPropertyEvent* pe = &next_event.xproperty;
        if (pe->state == PropertyDelete) {
            // no useful information for us
            continue;
        }
        if (pe->window != win) {
            fprintf(stderr, "Warning: expected event from window %u", (unsigned int)win);
            fprintf(stderr, " but got something from %u\n", (unsigned int)pe->window);
            continue;
        }
        XTextProperty text_prop;
        XGetTextProperty(g_display, win, &text_prop, pe->atom);
        char** list_return;
        int count;
        if (Success != Xutf8TextPropertyToTextList(g_display, &text_prop, &list_return, &count)) {
            XFree(text_prop.value);
            return 0;
        };
        bool print_signal = true;
        int i;
        for (i = 0; i < argc && i < count; i++) {
            if (0 != regexec(g_hook_regex + i, list_return[i], 0, NULL, 0)) {
                // found an regex that did not match
                // so skip this
                print_signal = false;
                break;
            }
        }
        if (print_signal) {
            // just print as list
            for (i = 0; i < count; i++) {
                printf("\"%s\", ", list_return[i]);
            }
            printf("\n");
            fflush(stdout);
        }
        // cleanup
        XFreeStringList(list_return);
        XFree(text_prop.value);
        if (!print_signal) {
            // if there was nothing printed
            // then act as there was no hook
            continue;
        }
        // check counter
        if (g_hook_count == 1) {
            break;
        } else if (g_hook_count > 1) {
            g_hook_count--;
        }
    }
    destroy_hook_regex();
    return 0;
}

int main(int argc, char* argv[]) {
    static struct option long_options[] = {
        {"no-newline", 0, 0, 'n'},
        {"wait", 0, 0, 'w'},
        {"count", 1, 0, 'c'},
        {"idle", 0, 0, 'i'},
        {0, 0, 0, 0}
    };
    // parse options
    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "+nwc:i", long_options, &option_index);
        if (c == -1) break;
        switch (c) {
            case 'i':
                g_hook_count = 0;
                g_wait_for_hook = 1;
                break;
            case 'c':
                g_hook_count = atoi(optarg);
                printf("setting to  %s\n", optarg);
                break;
            case 'w':
                g_wait_for_hook = 1;
                break;
            case 'n':
                g_ensure_newline = 0;
                break;
            default:
                fprintf(stderr, "unknown option `%s'\n", argv[optind]);
                exit(EXIT_FAILURE);
        }
    }
    int arg_index = optind; // index of the first-non-option argument
    // do communication
    dpy = XOpenDisplay(NULL);
    g_display = dpy;
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return EXIT_FAILURE;
    }
    root = DefaultRootWindow(dpy);
    int command_status;
    if (g_wait_for_hook == 1) {
        command_status = wait_for_hook(argc-arg_index, argv+arg_index);
    } else {
        command_status = send_command(argc-arg_index, argv+arg_index);
    }
    XCloseDisplay(dpy);
    return command_status;
}




