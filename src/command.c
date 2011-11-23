/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "ipc-protocol.h"
#include "command.h"
#include "utils.h"
#include "settings.h"
#include "layout.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>

enum {
    COMPLETION_LIST,
    COMPLETION_FUNCTION,
};


static char* completion_directions[] = {
    "left", "right", "down", "up", NULL
};

static char* completion_focus_args[] = {
    "-i", "-e", NULL
};

static char* completion_unrule_args[] = {
    "-F", "--all", NULL
};

static char* completion_flag_args[] = {
    "on", "off", "toggle", NULL
};

static char* completion_status[] = {
    "status", NULL
};

/* list of completions, if a line matches, then it will be used, the order
 * doesnot matter */
struct {
    char*   command;
    int     index;      /* which parameter to complete, command name is index = 0 */
    int     type;       /* which member of the method-union */
    union {
        void (*function)(int argc, char** argv, int pos, GString** output);
        char** list;
    }       method;
} g_completions[] = {
    { "add_monitor",2,  COMPLETION_FUNCTION, .method.function = complete_against_tags },
    { "dump",       1,  COMPLETION_FUNCTION, .method.function = complete_against_tags },
    { "floating",   1,  COMPLETION_FUNCTION, .method.function = complete_against_tags },
    { "floating",   1,      COMPLETION_LIST, .method.list = completion_flag_args },
    { "floating",   1,      COMPLETION_LIST, .method.list = completion_status },
    { "floating",   2,      COMPLETION_LIST, .method.list = completion_flag_args },
    { "floating",   2,      COMPLETION_LIST, .method.list = completion_status },
    { "focus",      1,      COMPLETION_LIST, .method.list = completion_directions },
    { "focus",      1,      COMPLETION_LIST, .method.list = completion_focus_args },
    { "focus",      2,      COMPLETION_LIST, .method.list = completion_directions },
    { "fullscreen", 1,      COMPLETION_LIST, .method.list = completion_flag_args },
    { "layout",     1,  COMPLETION_FUNCTION, .method.function = complete_against_tags },
    { "load",       1,  COMPLETION_FUNCTION, .method.function = complete_against_tags },
    { "move",       1,  COMPLETION_FUNCTION, .method.function = complete_against_tags },
    { "pseudotile", 1,      COMPLETION_LIST, .method.list = completion_flag_args },
    { "rename",     1,  COMPLETION_FUNCTION, .method.function = complete_against_tags },
    { "resize",     1,      COMPLETION_LIST, .method.list = completion_directions },
    { "shift",      1,      COMPLETION_LIST, .method.list = completion_directions },
    { "shift",      1,      COMPLETION_LIST, .method.list = completion_focus_args },
    { "shift",      2,      COMPLETION_LIST, .method.list = completion_directions },
    { "unrule",     1,      COMPLETION_LIST, .method.list = completion_unrule_args },
    { "use",        1,  COMPLETION_FUNCTION, .method.function = complete_against_tags },
    { 0 },
};

int call_command(int argc, char** argv, GString** output) {
    if (argc <= 0) {
        return HERBST_COMMAND_NOT_FOUND;
    }
    int i = 0;
    CommandBinding* bind = NULL;
    while (g_commands[i].cmd.standard != NULL) {
        if (!strcmp(g_commands[i].name, argv[0])) {
            // if command was found
            bind = g_commands + i;
            break;
        }
        i++;
    }
    if (!bind) {
        return HERBST_COMMAND_NOT_FOUND;
    }
    int status;
    if (bind->has_output) {
        status = bind->cmd.standard(argc, argv, output);
    } else {
        status = bind->cmd.no_output(argc, argv);
    }
    return status;
}

int call_command_no_ouput(int argc, char** argv) {
    GString* output = g_string_new("");
    int status = call_command(argc, argv, &output);
    g_string_free(output, true);
    return status;
}


int list_commands(int argc, char** argv, GString** output)
{
    int i = 0;
    while (g_commands[i].cmd.standard != NULL) {
        *output = g_string_append(*output, g_commands[i].name);
        *output = g_string_append(*output, "\n");
        i++;
    }
    return 0;
}

void complete_against_list(char* needle, char** list, GString** output) {
    size_t len = strlen(needle);
    while (*list) {
        char* name = *list;
        if (!strncmp(needle, name, len)) {
            *output = g_string_append(*output, name);
            *output = g_string_append(*output, "\n");
        }
        list++;
    }
}

void complete_against_tags(int argc, char** argv, int pos, GString** output) {
    char* needle = argv[pos];
    if (!needle) {
        needle = "";
    }
    size_t len = strlen(needle);
    for (int i = 0; i < g_tags->len; i++) {
        char* name = g_array_index(g_tags, HSTag*, i)->name->str;
        if (!strncmp(needle, name, len)) {
            *output = g_string_append(*output, name);
            *output = g_string_append(*output, "\n");
        }
    }
}

int complete_command(int argc, char** argv, GString** output) {
    // usage: complete POSITION command to complete ...
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }
    // index must be between first and als arg of "commmand to complete ..."
    int position = CLAMP(atoi(argv[1]), 0, argc-2);
    // complete command
    if (position == 0) {
        char* str = (argc >= 3) ? argv[2] : "";
        size_t len = strlen(str);
        int i = 0;
        while (g_commands[i].cmd.standard != NULL) {
            // only check the first len bytes
            if (!strncmp(str, g_commands[i].name, len)) {
                *output = g_string_append(*output, g_commands[i].name);
                *output = g_string_append(*output, "\n");
            }
            i++;
        }
    }
    if (argc >= 3) {
        char* str = (argc >= 4) ? argv[3] : "";
        size_t len = strlen(str);
        // complete parameters for commands
        bool is_toggle_command = !strcmp(argv[2], "toggle");
        if (position == 1 &&
            (!strcmp(argv[2], "set") || !strcmp(argv[2], "get")
            || is_toggle_command)) {
            // complete with setting name
            int i;
            for (i = 0; i < settings_count(); i++) {
                if (is_toggle_command && g_settings[i].type != HS_Int) {
                    continue;
                }
                // only check the first len bytes
                if (!strncmp(str, g_settings[i].name, len)) {
                    *output = g_string_append(*output, g_settings[i].name);
                    *output = g_string_append(*output, "\n");
                }
            }
        }
        else if ((position >= 1 && position <= 2
                    && !strcmp(argv[2], "merge_tag"))) {
            // we can complete first argument of use
            // or first and second argument of merge_tag
            bool is_merge_target = false;
            if (!strcmp(argv[2], "merge_tag") && position == 2) {
                // complete second arg to merge_tag
                str = (argc >= 5) ? argv[4] : "";
                len = strlen(str);
                is_merge_target = true;
            }
            // list tags
            int i;
            for (i = 0; i < g_tags->len; i++) {
                char* name = g_array_index(g_tags, HSTag*, i)->name->str;
                if (is_merge_target && !strcmp(name, argv[3])) {
                    // merge target must not be equal to tag to remove
                    continue;
                }
                if (!strncmp(str, name, len)) {
                    *output = g_string_append(*output, name);
                    *output = g_string_append(*output, "\n");
                }
            }
        }
        else {
            for (int i = 0; i < LENGTH(g_completions); i++) {
                if (!g_completions[i].command
                    || position != g_completions[i].index
                    || strcmp(argv[2], g_completions[i].command)) {
                    continue;
                }
                char* needle = argv[position + 2];
                if (!needle) {
                    needle = "";
                }
                // try to complete
                switch (g_completions[i].type) {
                    case COMPLETION_FUNCTION:
                        g_completions[i].method.function(argc - 2, argv + 2, position, output);
                        break;
                    case COMPLETION_LIST:
                        complete_against_list(needle,
                            g_completions[i].method.list, output);
                        break;
                }
            }
        }
    }
    return 0;
}





