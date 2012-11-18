/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <signal.h>
#include <regex.h>
#include <assert.h>

#include "ipc-client.h"
#include "../src/globals.h"
#include "../src/utils.h"
#include "../src/ipc-protocol.h"

void print_help(char* command);
void init_hook_regex(int argc, char* argv[]);
void destroy_hook_regex();

int g_ensure_newline = 1; // if set, output ends with a newline
int g_wait_for_hook = 0; // if set, do not execute command but wait
bool g_quiet = false;
regex_t* g_hook_regex = NULL;
int g_hook_regex_count = 0;
int g_hook_count = 1; // count of hooks to wait for, 0 means: forever

static void quit_herbstclient(int signal) {
    // TODO: better solution to quit x connection more softly?
    fprintf(stderr, "interrupted by signal %d\n", signal);
    destroy_hook_regex();
    exit(EXIT_FAILURE);
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


void print_help(char* command) {
    // Eventually replace this and the option parsing with some fancy macro
    // based thing? Is the cost of maintainance really that high?

    fprintf(stdout,
        "Usage: %s [OPTIONS] COMMAND [ARGS ...]\n"
        "       %s [OPTIONS] [--wait|--idle] [FILTER ...]\n",
        command, command);

    char* help_string =
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
    fputs(help_string, stdout);
}

int main_hook(int argc, char* argv[]) {
    init_hook_regex(argc, argv);
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        if (!g_quiet) {
            fprintf(stderr, "Cannot open display\n");
        }
        return EXIT_FAILURE;
    }
    HCConnection* con = hc_connect_to_display(display);
    signal(SIGTERM, quit_herbstclient);
    signal(SIGINT,  quit_herbstclient);
    signal(SIGQUIT, quit_herbstclient);
    while (1) {
        bool print_signal = true;
        int hook_argc;
        char** hook_argv;
        if (!hc_next_hook(con, &hook_argc, &hook_argv)) {
            fprintf(stderr, "Cannot listen for hooks\n");
            destroy_hook_regex();
            return EXIT_FAILURE;
        }
        for (int i = 0; i < argc && i < hook_argc; i++) {
            if (0 != regexec(g_hook_regex + i, hook_argv[i], 0, NULL, 0)) {
                // found an regex that did not match
                // so skip this
                print_signal = false;
                break;
            }
        }
        if (print_signal) {
            // just print as list
            for (int i = 0; i < hook_argc; i++) {
                printf("%s%s", i ? "\t" : "", hook_argv[i]);
            }
            printf("\n");
            fflush(stdout);
        }
        argv_free(hook_argc, hook_argv);
        if (print_signal) {
            // check counter
            if (g_hook_count == 1) {
                break;
            } else if (g_hook_count > 1) {
                g_hook_count--;
            }
        }
    }
    hc_disconnect(con);
    XCloseDisplay(display);
    destroy_hook_regex();
    return 0;
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
                print_help(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                fprintf(stderr, "unknown option `%s'\n", argv[optind]);
                exit(EXIT_FAILURE);
        }
    }
    int arg_index = optind; // index of the first-non-option argument
    // do communication
    int command_status;
    if (g_wait_for_hook == 1) {
        // install signals
        command_status = main_hook(argc-arg_index, argv+arg_index);
    } else {
        GString* output;
        bool suc = hc_send_command_once(argc-arg_index, argv+arg_index,
                                        &output, &command_status);
        if (!suc) {
            fprintf(stderr, "Error: Could not send command.\n");
            return EXIT_FAILURE;
        }
        FILE* file = stdout; // on success, output to stdout
        if (command_status != 0) { // any error, output to stderr
            file = stderr;
        }
        if (g_ensure_newline) {
            if (output->len > 0 && output->str[output->len - 1] != '\n') {
                fputs("\n", file);
            }
        }
        if (command_status == HERBST_NEED_MORE_ARGS) { // needs more arguments
            fprintf(stderr, "%s: not enough arguments\n", argv[arg_index]); // first argument == cmd
        }
        g_string_free(output, true);
    }
    return command_status;
}

