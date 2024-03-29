#include <X11/Xlib.h>
#include <assert.h>
#include <getopt.h>
#include <regex.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include "../src/ipc-protocol.h"
#include "client-utils.h"
#include "ipc-client.h"

static void print_help(char* command, FILE* file);
static void init_hook_regex(int argc, char* argv[]);
static void destroy_hook_regex();
static bool trailing_newline_missing(const char* text);

static int g_ensure_newline = 1; // if set, output ends with a newline
static bool g_null_char_as_delim = false; // if true, the null character is used as delimiter
static bool g_print_last_arg_only = false; // if true, prints only the last argument of a hook
static int g_wait_for_hook = 0; // if set, do not execute command but wait
static bool g_quiet = false;
static regex_t* g_hook_regex = NULL;
static int g_hook_regex_count = 0;
static int g_hook_count = 1; // count of hooks to wait for, 0 means: forever

static void quit_herbstclient(int signal) {
    // TODO: better solution to quit x connection more softly?
    fprintf(stderr, "interrupted by signal %d\n", signal);
    destroy_hook_regex();
    exit(EXIT_FAILURE);
}

void init_hook_regex(int argc, char* argv[]) {
    if (argc <= 0) {
        return;
    }
    g_hook_regex = (regex_t*)malloc(sizeof(regex_t)*argc);
    assert(g_hook_regex != NULL);
    int i;
    // create all regexes
    for (i = 0; i < argc; i++) {
        int status = regcomp(g_hook_regex + i, argv[i], REG_NOSUB|REG_EXTENDED);
        if (status != 0) {
            char buf[1000];
            regerror(status, g_hook_regex + i, buf, sizeof(buf));
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


void print_help(char* command, FILE* file) {
    // Eventually replace this and the option parsing with some fancy macro
    // based thing? Is the cost of maintainance really that high?

    fprintf(file,
        "Usage: %s [OPTIONS] COMMAND [ARGS ...]\n"
        "       %s [OPTIONS] [--wait|--idle] [FILTER ...]\n",
        command, command);

    char* help_string =
        "Send a COMMAND with optional arguments ARGS to a running "
        "herbstluftwm instance.\n\n"
        "Options:\n"
        "\t-n, --no-newline: Do not print a newline if output does not end "
            "with a newline.\n"
        "\t-0, --print0: Use the null character as delimiter between the "
            "output of hooks.\n"
        "\t-l, --last-arg: Print only the last argument of a hook.\n"
        "\t-i, --idle: Wait for hooks instead of executing commands.\n"
        "\t-w, --wait: Same as --idle but exit after first --count hooks.\n"
        "\t-c, --count COUNT: Let --wait exit after COUNT hooks were "
            "received and printed. The default of COUNT is 1.\n"
        "\t-q, --quiet: Do not print error messages if herbstclient cannot "
            "connect to the running herbstluftwm instance.\n"
        "\t--binary-pipe: run multiple commands via a binary interface"
            "on the standard channels.\n"
        "\t-v, --version: Print the herbstclient version. To get the "
            "herbstluftwm version, use 'herbstclient version'.\n"
        "\t-h, --help: Print this help."
        "\n"
        "See the man page (herbstclient(1)) for more details.\n";
    fputs(help_string, file);
}

int main_hook(int argc, char* argv[]) {
    init_hook_regex(argc, argv);
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        if (!g_quiet) {
            fprintf(stderr, "Error: Cannot open display\n");
        }
        destroy_hook_regex();
        return EXIT_FAILURE;
    }
    HCConnection* con = hc_connect_to_display(display);
    if (!hc_check_running(con)) {
        if (!g_quiet) {
            fprintf(stderr, "Error: herbstluftwm is not running\n");
        }
        hc_disconnect(con);
        XCloseDisplay(display);
        destroy_hook_regex();
        return EXIT_FAILURE;
    }
    signal(SIGTERM, quit_herbstclient);
    signal(SIGINT,  quit_herbstclient);
    signal(SIGQUIT, quit_herbstclient);
    int exit_code = 0;
    while (1) {
        bool print_signal = true;
        int hook_argc;
        char** hook_argv;
        if (!hc_next_hook(con, &hook_argc, &hook_argv)) {
            fprintf(stderr, "Cannot listen for hooks\n");
            exit_code = EXIT_FAILURE;
            // clean up HCConnection and regexes before
            // returning
            break;
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
            if (g_print_last_arg_only) {
                // just drop hooks without content
                if (hook_argc >= 1) {
                    printf("%s", hook_argv[hook_argc-1]);
                }
            } else {
                // just print as list
                for (int i = 0; i < hook_argc; i++) {
                    printf("%s%s", i ? "\t" : "", hook_argv[i]);
                }
            }
            if (g_null_char_as_delim) {
                putchar(0);
            } else {
                printf("\n");
            }
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
    return exit_code;
}

static bool main_binary_pipe_loop(HCConnection* con) {
    int hc_fd = hc_connection_socket(con);
    int stdin_fd = STDIN_FILENO;
    int maximum_plus_1 = 1 + ((hc_fd > stdin_fd) ? hc_fd : stdin_fd);
    fd_set in_fds;
    bool running = true;
    // (void) freopen(NULL, "rb", stdin);
    FILE* stdout_bin = freopen(NULL, "wb", stdout);
    while (running) {
        ArgList* command_args = arglist_new();
        bool command_complete = false;
        while (!command_complete && running) {
            FD_ZERO(&in_fds);
            FD_SET(hc_fd, &in_fds);
            FD_SET(stdin_fd, &in_fds);
            // wait for an event or a signal
            select(maximum_plus_1, &in_fds, NULL, NULL, NULL);
            if (FD_ISSET(hc_fd, &in_fds)) {
                hc_process_events(con);
                if (!hc_check_running(con)) {
                    fprintf(stderr, "herbstluftwm exited\n");
                    running = false;
                    break;
                }
            }
            if (FD_ISSET(stdin_fd, &in_fds)) {
                // // debuging output, maybe it is useful later:
                // fprintf(stderr, "Reading the next command...\n");
                char* token = read_until_null_byte(stdin_fd);
                if (!token) {
                    running = false;
                    break;
                } else if (!strcmp(token, "ARG")) {
                    char* arg = read_until_null_byte(stdin_fd);
                    if (arg) {
                        // // debuging output, maybe it is useful later:
                        // fprintf(stderr, "arg (%s)\n", arg);
                        arglist_push_with_ownership(command_args, arg);
                    } else {
                        running = false;
                    }
                } else if (!strcmp(token, "RUN")) {
                    command_complete = true;
                } else {
                    fprintf(stderr, "Invalid token (%s)\n", token);
                    running = false;
                }
                free(token);
            }
        }
        if (!running) {
            arglist_free(command_args);
            break;
        }
        // run the command
        char* output = NULL;
        char* error = NULL;
        int status = 0;
        bool suc = hc_send_command(con,
                                   command_args->null_index, command_args->data,
                                   &output, &error, &status);
        arglist_free(command_args);
        if (!suc) {
            fprintf(stderr, "Error: Could not send command.\n");
            return false;
        }
        // // debuging output, maybe it is useful later:
        // fprintf(stderr, "stdout=\"%s\"\n", output);
        // fprintf(stderr, "stderr=\"%s\"\n", error);
        // fprintf(stderr, "status=\"%d\"\n", status);
        fprintf(stdout_bin, "STDOUT%c%s%c", 0, output, 0);
        fprintf(stdout_bin, "STDERR%c%s%c", 0, error, 0);
        fprintf(stdout_bin, "STATUS%c%d%c", 0, status, 0);
        fflush(stdout_bin);
    }
    return true;
}

/**
 * @brief checks whether the text ends with a line
 * that misses a newline character
 * @param the text
 * @return whether there are characters in the text
 * not enclosed by a newline character.
 */
static bool trailing_newline_missing(const char* text) {
    size_t len = strlen(text);
    if (len == 0) {
        // no lines, so no open lines
        return false;
    }
    return text[len - 1] != '\n';
}

int main(int argc, char* argv[]) {
    int binary_pipe = 0;
    struct option long_options[] = {
        {"no-newline", 0, 0, 'n'},
        {"print0", 0, 0, '0'},
        {"last-arg", 0, 0, 'l'},
        {"binary-pipe", 0, &binary_pipe, 1},
        {"wait", 0, 0, 'w'},
        {"count", 1, 0, 'c'},
        {"idle", 0, 0, 'i'},
        {"quiet", 0, 0, 'q'},
        {"version", 0, 0, 'v'},
        {"help", 0, 0, 'h'},
        {0, 0, 0, 0}
    };
    // parse options
    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "+n0lwc:iqhv", long_options, &option_index);
        if (c == -1) {
            break;
        }
        switch (c) {
            case 0:
                /* ignore recognized long option */
                break;
            case 'i':
                g_hook_count = 0;
                g_wait_for_hook = 1;
                break;
            case 'c':
                g_hook_count = atoi(optarg);
                break;
            case 'w':
                g_wait_for_hook = 1;
                break;
            case 'n':
                g_ensure_newline = 0;
                break;
            case '0':
                g_null_char_as_delim = true;
                break;
            case 'l':
                g_print_last_arg_only = true;
                break;
            case 'q':
                g_quiet = true;
                break;
            case 'h':
                print_help(argv[0], stdout);
                exit(EXIT_SUCCESS);
            case 'v':
                fputs("herbstclient " HERBSTLUFT_VERSION "\n", stdout);
                exit(EXIT_SUCCESS);
            default:
                exit(EXIT_FAILURE);
        }
    }
    int arg_index = optind; // index of the first-non-option argument
    if ((argc - arg_index == 0) && !g_wait_for_hook && !binary_pipe) {
        // if there are no non-option arguments, and no --idle/--wait, display
        // the help and exit
        fprintf(stderr, "Error: COMMAND, --wait, --idle, or --binary-pipe missing.\n");
        print_help(argv[0], stderr);
        exit(EXIT_FAILURE);
    }
    // do communication
    int command_status = EXIT_FAILURE; // just a fallback
    if (g_wait_for_hook == 1) {
        // install signals
        command_status = main_hook(argc-arg_index, argv+arg_index);
    } else {
        char* output = NULL;
        char* error = NULL;
        HCConnection* con = hc_connect();
        if (!con) {
            if (!g_quiet) {
                fprintf(stderr, "Error: Cannot open display.\n");
            }
            return EXIT_FAILURE;
        }
        if (!hc_check_running(con)) {
            if (!g_quiet) {
                fprintf(stderr, "Error: herbstluftwm is not running.\n");
            }
            hc_disconnect(con);
            return EXIT_FAILURE;
        }
        bool suc;
        if (binary_pipe) {
            suc = main_binary_pipe_loop(con);
        } else {
            suc = hc_send_command(con, argc-arg_index, argv+arg_index,
                                       &output, &error, &command_status);
            if (command_status == HERBST_NEED_MORE_ARGS) { // needs more arguments
                fprintf(stderr, "%s: not enough arguments\n", argv[arg_index]); // first argument == cmd
            }
        }
        hc_disconnect(con);
        if (!suc) {
            if (!g_quiet) {
                fprintf(stderr, "Error: Could not send command.\n");
            }
            return EXIT_FAILURE;
        }
        if (error) {
            // usually, it makes more sense to print the error first.
            fputs(error, stderr);
            if (trailing_newline_missing(error)) {
                // always ensure that the error messages
                // are enclosed with newlines
                fputs("\n", stderr);
            }
            free(error);
        }
        if (output) {
            fputs(output, stdout);
            if (g_ensure_newline && trailing_newline_missing(output)) {
                fputs("\n", stdout);
            }
            free(output);
        }
    }
    return command_status;
}

