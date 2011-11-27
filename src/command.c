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

static char* completion_directions[]    = { "left", "right", "down", "up",NULL};
static char* completion_focus_args[]    = { "-i", "-e", NULL };
static char* completion_unrule_args[]   = { "-F", "--all", NULL };
static char* completion_flag_args[]     = { "on", "off", "toggle", NULL };
static char* completion_status[]        = { "status", NULL };

static bool no_completion(int argc, char** argv, int pos) {
    return false;
}

static bool first_parameter_is_tag(int argc, char** argv, int pos);

/* find out, if a parameter still expects a parameter at a certain index.
 * only if this returns true, than a completion will be searched.
 *
 * if no match is found, then it defaults to "command still expects a
 * parameter".
 */
struct {
    char*   command;    /* the first argument */
    int     min_index;  /* rule will only be considered */
                        /* if current pos >= min_index */
    bool    (*function)(int argc, char** argv, int pos);
} g_parameter_expected[] = {
    { "quit",           0,  no_completion },
    { "reload",         0,  no_completion },
    { "add_monitor",    7,  no_completion },
    { "dump",           2,  no_completion },
    { "floating",       3,  no_completion },
    { "floating",       2,  first_parameter_is_tag },
    { 0 },
};

/* list of completions, if a line matches, then it will be used, the order
 * doesnot matter */
struct {
    char*   command;
    int     index;      /* which parameter to complete */
                        /* command name is index = 0 */
    /* === various methods, how to complete === */
    /* completion by function */
    void (*function)(int argc, char** argv, int pos, GString** output);
    /* completion by a list of strings */
    char** list;
} g_completions[] = {
    /* name ,       index,  completion method                   */
    { "add_monitor",    2,  .function = complete_against_tags },
    { "dump",           1,  .function = complete_against_tags },
    { "floating",       1,  .function = complete_against_tags },
    { "floating",       1,  .list = completion_flag_args },
    { "floating",       1,  .list = completion_status },
    { "floating",       2,  .list = completion_flag_args },
    { "floating",       2,  .list = completion_status },
    { "focus",          1,  .list = completion_directions },
    { "focus",          1,  .list = completion_focus_args },
    { "focus",          2,  .list = completion_directions },
    { "fullscreen",     1,  .list = completion_flag_args },
    { "layout",         1,  .function = complete_against_tags },
    { "load",           1,  .function = complete_against_tags },
    { "move",           1,  .function = complete_against_tags },
    { "pseudotile",     1,  .list = completion_flag_args },
    { "rename",         1,  .function = complete_against_tags },
    { "resize",         1,  .list = completion_directions },
    { "shift",          1,  .list = completion_directions },
    { "shift",          1,  .list = completion_focus_args },
    { "shift",          2,  .list = completion_directions },
    { "unrule",         1,  .list = completion_unrule_args },
    { "use",            1,  .function = complete_against_tags },
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
    char* needle;
    if (pos >= argc) {
        needle = "";
    } else {
        needle = argv[pos];
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

bool parameter_expected(int argc, char** argv, int pos) {
    if (pos <= 0 || argc < 1) {
        /* no parameter if there is no command */
        return false;
    }
    for (int i = 0; i < LENGTH(g_parameter_expected)
                    && g_parameter_expected[i].command; i++) {
        if (pos < g_parameter_expected[i].min_index) {
            continue;
        }
        if (!strcmp(g_parameter_expected[i].command, argv[0])) {
            return g_parameter_expected[i].function(argc, argv, pos);
        }
    }
    return true;
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
        return 0;
    }
    if (!parameter_expected(argc - 2, argv + 2, position)) {
        return HERBST_NO_PARAMETER_EXPECTED;
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
                if (g_completions[i].function) {
                    g_completions[i].function(argc - 2, argv + 2,
                                                     position, output);
                }
                if (g_completions[i].list) {
                    complete_against_list(needle, g_completions[i].list,
                                          output);
                }
            }
        }
    }
    return 0;
}

bool first_parameter_is_tag(int argc, char** argv, int pos) {
    // only complete if first parameter is a valid tag
    if (argc >= 2 && find_tag(argv[1]) && pos == 2) {
        return true;
    } else {
        return false;
    }
}

