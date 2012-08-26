/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <signal.h>

#include "ipc-client.h"
#include "../src/globals.h"

void print_help(char* command);

int g_ensure_newline = 1; // if set, output ends with a newline
int g_wait_for_hook = 0; // if set, do not execute command but wait

static void quit_herbstclient(int signal) {
    // TODO: better solution to quit x connection more softly?
    fprintf(stderr, "interrupted by signal %d\n", signal);
    destroy_hook_regex();
    exit(EXIT_FAILURE);
}

void print_help(char* command) {
    // Eventually replace this and the option parsing with some fancy macro
    // based thing? Is the cost of maintainence really that high?

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
        g_display = XOpenDisplay(NULL);
        if (!g_display) {
            if (!g_quiet) {
                fprintf(stderr, "Cannot open display\n");
            }
            return EXIT_FAILURE;
        }
        // install signals
        signal(SIGTERM, quit_herbstclient);
        signal(SIGINT, quit_herbstclient);
        signal(SIGQUIT, quit_herbstclient);
        command_status = wait_for_hook(argc-arg_index, argv+arg_index);
        XCloseDisplay(g_display);
    } else {
        GString* output;
        bool suc = hc_send_command_once(argc-arg_index, argv+arg_index,
                                        &output, &command_status);
        if (!suc) {
            fprintf(stderr, "Error: Could not send command.\n");
            return EXIT_FAILURE;
        }
        fputs(output->str, stdout);
        if (g_ensure_newline) {
            if (output->len > 0 && output->str[output->len - 1] != '\n') {
                fputs("\n", stdout);
            }
        }
        g_string_free(output, true);
    }
    return command_status;
}

