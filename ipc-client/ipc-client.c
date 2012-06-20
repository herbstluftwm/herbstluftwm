/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
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
void print_help(void);


Display* g_display;
Window root;
int g_ensure_newline = 1; // if set, output ends with an newline
int g_wait_for_hook = 0; // if set, do not execute command but wait
int g_hook_count = 1; // count of hooks to wait for, 0 means: forever
regex_t* g_hook_regex = NULL;
int g_hook_regex_count = 0;
bool g_quiet = false;

void init_hook_regex(int argc, char* argv[]);
void destroy_hook_regex();
Window get_hook_window();

static void quit_herbstclient(int signal) {
    // TODO: better solution to quit x connection more softly?
    fprintf(stderr, "interrupted by signal %d\n", signal);
    destroy_hook_regex();
    exit(EXIT_FAILURE);
}

int send_command(int argc, char* argv[]) {
    // check for running window manager instance
    if (!get_hook_window()) {
        return EXIT_FAILURE;
    }
    /* ensure that classhint and the command is set when the hlwm-server
     * receives the XCreateWindowEvent */
    XGrabServer(g_display);
    // create window
    Window win = XCreateSimpleWindow(g_display, root, 42, 42, 42, 42, 0, 0, 0);
    // set wm_class for window
    XClassHint *hint = XAllocClassHint();
    hint->res_name = HERBST_IPC_CLASS;
    hint->res_class = HERBST_IPC_CLASS;
    XSetClassHint(g_display, win, hint);
    XFree(hint);
    XSelectInput(g_display, win, PropertyChangeMask);
    // set arguments
    XTextProperty text_prop;
    Atom atom = ATOM(HERBST_IPC_ARGS_ATOM);
    Xutf8TextListToTextProperty(g_display, argv, argc, XUTF8StringStyle, &text_prop);
    XSetTextProperty(g_display, win, &text_prop, atom);
    /* the window has been initialized properly, now allow the server to
     * receive the event for it */
    XUngrabServer(g_display);
    XFree(text_prop.value);
    // get output
    int command_status = 0;
    XEvent event;
    GString* output = NULL;
    bool output_received = false, status_received = false;
    while (!output_received || !status_received) {
        XNextEvent(g_display, &event);
        if (event.type != PropertyNotify) {
            // got an event of wrong type
            continue;
        }
        XPropertyEvent* pe = &(event.xproperty);
        if (pe->window != win) {
            // got an event from wrong window
            continue;
        }
        if (!output_received
            && !strcmp(XGetAtomName(g_display, pe->atom), HERBST_IPC_OUTPUT_ATOM)) {
            output = window_property_to_g_string(g_display, win, ATOM(HERBST_IPC_OUTPUT_ATOM));
            if (!output) die("could not get WindowProperty \"%s\"\n", HERBST_IPC_OUTPUT_ATOM);
            output_received = true;
        }
        else if (!status_received && !strcmp(
                     XGetAtomName(g_display, pe->atom),
                     HERBST_IPC_STATUS_ATOM)) {
            int *value;
            Atom type;
            int format;
            unsigned long items, bytes;
            if (Success != XGetWindowProperty(g_display, win,
                    ATOM(HERBST_IPC_STATUS_ATOM), 0, 1, False,
                    XA_ATOM, &type, &format, &items, &bytes, (unsigned char**)&value)) {
                    // if could not get window property
                die("could not get WindowProperty \"%s\"\n", HERBST_IPC_STATUS_ATOM);
            }
            command_status = *value;
            XFree(value);
            status_received = true;
        }
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
    XDestroyWindow(g_display, win);
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

Window get_hook_window() {
    int *value; // list of ints
    Atom type;
    int format;
    unsigned long items, bytes;
    int status = XGetWindowProperty(g_display, root,
        ATOM(HERBST_HOOK_WIN_ID_ATOM), 0, 1, False,
        XA_ATOM, &type, &format, &items, &bytes, (unsigned char**)&value);
    // only accept exactly one Window id
    if (status != Success || items != 1) {
        if (!g_quiet) {
            fprintf(stderr, "no running herbstluftwm detected\n");
        }
        return 0;
    }
    Window win = *value;
    XFree(value);
    return win;
}

int wait_for_hook(int argc, char* argv[]) {
    // install signals
    signal(SIGTERM, quit_herbstclient);
    signal(SIGINT, quit_herbstclient);
    signal(SIGQUIT, quit_herbstclient);
    init_hook_regex(argc, argv);
    // get window to listen at
    Window win = get_hook_window();
    if (!win) {
        return EXIT_FAILURE;
    }
    // listen on window
    XSelectInput(g_display, win, StructureNotifyMask|PropertyChangeMask);
    XEvent next_event;
    while (1) {
        XNextEvent(g_display, &next_event);
        if (next_event.type == DestroyNotify) {
            if (next_event.xdestroywindow.window == win) {
                // hook window was destroyed
                // so quit idling
                break;
            }
        }
        if (next_event.type != PropertyNotify) {
            fprintf(stderr, "Warning: got other event than PropertyNotify\n");
            continue;
        }
        XPropertyEvent* pe = &next_event.xproperty;
        if (pe->state == PropertyDelete) {
            // just ignore property delete events
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
                printf("%s%s", i ? "\t" : "", list_return[i]);
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

void print_help(void) {
    // Eventually replace this and the option parsing with some fancy macro
    // based thing? Is the cost of maintainence really that high?

    char *help_string =
        "Usage: herbstclient [OPTIONS] COMMAND [ARGS ...]\n"
        "       herbstclient [OPTIONS] [--wait|--idle] [FILTER ...]\n"
        "Send a COMMAND with optional arguments ARGS to a running "
        "herbstluftwm instance.\n\n"
        "Options:\n"
        "\t-n, --no-newline: Do not print a newline if output does not end "
            "with a newline.\n"
        "\t-i, --idle: Wait for hooks instead of executing commands.\n"
        "\t-w, --wait: Same as --idle but exit after first --count hooks.\n"
        "\t-c, --count COUNT: Let --wait exit after COUNT hooks were "
            "received and printed. The default of COUNT is 1.\n"
        "\t-q, --quiet: Do not print error messages if herbstclient cannot "
            "connect to the running herbstluftwm instance.\n"
        "\t-h, --help: Print this help."
        "\n"
        "See the man page (herbstclient(1)) for more details.\n";
    fprintf(stdout, help_string);
}
int main(int argc, char* argv[]) {
    static struct option long_options[] = {
        {"no-newline", 0, 0, 'n'},
        {"wait", 0, 0, 'w'},
        {"count", 1, 0, 'c'},
        {"idle", 0, 0, 'i'},
        {"quiet", 0, 0, 'q'},
        {"help", 0, 0, 'h'},
        {0, 0, 0, 0}
    };
    // parse options
    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "+nwc:iqh", long_options, &option_index);
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
            case 'q':
                g_quiet = true;
                break;
            case 'h':
                print_help();
                exit(EXIT_SUCCESS);
            default:
                fprintf(stderr, "unknown option `%s'\n", argv[optind]);
                exit(EXIT_FAILURE);
        }
    }
    int arg_index = optind; // index of the first-non-option argument
    // do communication
    g_display = XOpenDisplay(NULL);
    if (!g_display) {
        if (!g_quiet) {
            fprintf(stderr, "Cannot open display\n");
        }
        return EXIT_FAILURE;
    }
    root = DefaultRootWindow(g_display);
    int command_status;
    if (g_wait_for_hook == 1) {
        command_status = wait_for_hook(argc-arg_index, argv+arg_index);
    } else {
        command_status = send_command(argc-arg_index, argv+arg_index);
    }
    XCloseDisplay(g_display);
    return command_status;
}
