/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "ipc-protocol.h"
#include "command.h"
#include "utils.h"
#include "settings.h"
#include "layout.h"
#include "key.h"
#include "clientlist.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <search.h>

static char* completion_directions[]    = { "left", "right", "down", "up",NULL};
static char* completion_focus_args[]    = { "-i", "-e", NULL };
static char* completion_unrule_args[]   = { "-F", "--all", NULL };
static char* completion_keyunbind_args[]= { "-F", "--all", NULL };
static char* completion_flag_args[]     = { "on", "off", "toggle", NULL };
static char* completion_status[]        = { "status", NULL };
static char* completion_special_winids[]= { "urgent", "", NULL };
static char* completion_use_index_args[]= { "--skip-visible", NULL };
static char* completion_cycle_all_args[]= { "--skip-invisible", NULL };
static char* completion_pm_one[]= { "+1", "-1", NULL };
static char* completion_split_modes[]= { "horizontal", "vertical", NULL };
static char* completion_split_ratios[]= {
    "0.1", "0.2", "0.3", "0.4", "0.5", "0.6", "0.7", "0.8", "0.9", NULL };

static bool no_completion(int argc, char** argv, int pos) {
    return false;
}

static bool first_parameter_is_tag(int argc, char** argv, int pos);
static bool first_parameter_is_flag(int argc, char** argv, int pos);
static bool keybind_parameter_expected(int argc, char** argv, int pos);

/* find out, if a command still expects a parameter at a certain index.
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
    { "quit",           1,  no_completion },
    { "reload",         1,  no_completion },
    { "version",        1,  no_completion },
    { "list_commands",  1,  no_completion },
    { "list_monitors",  1,  no_completion },
    { "list_keybinds",  1,  no_completion },
    { "lock",           1,  no_completion },
    { "unlock",         1,  no_completion },
    { "keybind",        2,  keybind_parameter_expected },
    { "keyunbind",      2,  no_completion },
    { "mousebind",      3,  no_completion },
    { "mouseunbind",    1,  no_completion },
    { "focus_nth",      2,  no_completion },
    { "cycle",          2,  no_completion },
    { "cycle_all",      3,  no_completion },
    { "cycle_layout",   LAYOUT_COUNT+2, no_completion },
    { "set_layout",     2,  no_completion },
    { "close",          1,  no_completion },
    { "close_or_remove",1,  no_completion },
    { "split",          3,  no_completion },
    { "focus",          3,  no_completion },
    { "focus",          2,  first_parameter_is_flag },
    { "raise",          2,  no_completion },
    { "jumpto",         2,  no_completion },
    { "bring",          2,  no_completion },
    { "resize",         3,  no_completion },
    { "focus_edge",     2,  no_completion },
    { "shift_edge",     2,  no_completion },
    { "shift",          3,  no_completion },
    { "shift",          2,  first_parameter_is_flag },
    { "remove",         1,  no_completion },
    { "rotate",         1,  no_completion },
    { "set",            3,  no_completion },
    { "get",            2,  no_completion },
    { "toggle",         2,  no_completion },
    { "cycle_monitor",  2,  no_completion },
    { "focus_monitor",  2,  no_completion },
    { "add",            2,  no_completion },
    { "use",            2,  no_completion },
    { "use_index",      3,  no_completion },
    { "merge_tag",      3,  no_completion },
    { "rename",         3,  no_completion },
    { "move",           2,  no_completion },
    { "move_index",     3,  no_completion },
    { "lock_tag",       2,  no_completion },
    { "unlock_tag",     2,  no_completion },
    { "detect_monitors",1,  no_completion },
    { "add_monitor",    7,  no_completion },
    { "remove_monitor", 2,  no_completion },
    { "move_monitor",   7,  no_completion },
    { "raise_monitor",  2,  no_completion },
    { "stack",          2,  no_completion },
    { "monitor_rect",   3,  no_completion },
    { "pad",            6,  no_completion },
    { "list_padding",   1,  no_completion },
    { "layout",         3,  no_completion },
    { "dump",           3,  no_completion },
    { "load",           3,  no_completion },
    { "load",           2,  first_parameter_is_tag },
    { "tag_status",     2,  no_completion },
    { "floating",       3,  no_completion },
    { "floating",       2,  first_parameter_is_tag },
    { "unrule",         2,  no_completion },
    { "fullscreen",     2,  no_completion },
    { "pseudotile",     2,  no_completion },
    { "getenv",         2,  no_completion },
    { "setenv",         3,  no_completion },
    { "unsetenv",       2,  no_completion },
    { 0 },
};

/* list of completions, if a line matches, then it will be used, the order
 * does not matter */
struct {
    char*   command;
    enum {
        LE, /* lower equal */
        EQ, /* equal to */
        GE, /* greater equal */
    } relation;         /* defines how the index matches */
    int     index;      /* which parameter to complete */
                        /* command name is index = 0 */
                        /* GE 0 matches any position */
                        /* LE 3 matches position from 0 to 3 */
    /* === various methods, how to complete === */
    /* completion by function */
    void (*function)(int argc, char** argv, int pos, GString* output);
    /* completion by a list of strings */
    char** list;
} g_completions[] = {
    /* name , relation, index,  completion method                   */
    { "add_monitor",    EQ, 2,  .function = complete_against_tags },
    { "and",            GE, 1,  .function = complete_chain },
    { "bring",          EQ, 1,  .list = completion_special_winids },
    { "bring",          EQ, 1,  .function = complete_against_winids },
    { "cycle",          EQ, 1,  .list = completion_pm_one },
    { "chain",          GE, 1,  .function = complete_chain },
    { "cycle_all",      EQ, 1,  .list = completion_cycle_all_args },
    { "cycle_all",      EQ, 1,  .list = completion_pm_one },
    { "cycle_all",      EQ, 2,  .list = completion_pm_one },
    { "cycle_monitor",  EQ, 1,  .list = completion_pm_one },
    { "dump",           EQ, 1,  .function = complete_against_tags },
    { "floating",       EQ, 1,  .function = complete_against_tags },
    { "floating",       EQ, 1,  .list = completion_flag_args },
    { "floating",       EQ, 1,  .list = completion_status },
    { "floating",       EQ, 2,  .list = completion_flag_args },
    { "floating",       EQ, 2,  .list = completion_status },
    { "focus",          EQ, 1,  .list = completion_directions },
    { "focus",          EQ, 1,  .list = completion_focus_args },
    { "focus",          EQ, 2,  .list = completion_directions },
    { "fullscreen",     EQ, 1,  .list = completion_flag_args },
    { "layout",         EQ, 1,  .function = complete_against_tags },
    { "load",           EQ, 1,  .function = complete_against_tags },
    { "merge_tag",      EQ, 1,  .function = complete_against_tags },
    { "merge_tag",      EQ, 2,  .function = complete_merge_tag },
    { "move",           EQ, 1,  .function = complete_against_tags },
    { "move_index",     EQ, 2,  .list = completion_use_index_args },
    { "or",             GE, 1,  .function = complete_chain },
    { "!",              GE, 1,  .function = complete_negate },
    { "pseudotile",     EQ, 1,  .list = completion_flag_args },
    { "keybind",        GE, 2,  .function = complete_against_keybind_command },
    { "keyunbind",      EQ, 1,  .list = completion_keyunbind_args },
    { "keyunbind",      EQ, 1,  .function = complete_against_keybinds },
    { "rename",         EQ, 1,  .function = complete_against_tags },
    { "raise",          EQ, 1,  .list = completion_special_winids },
    { "raise",          EQ, 1,  .function = complete_against_winids },
    { "jumpto",         EQ, 1,  .list = completion_special_winids },
    { "jumpto",         EQ, 1,  .function = complete_against_winids },
    { "resize",         EQ, 1,  .list = completion_directions },
    { "shift_edge",     EQ, 1,  .list = completion_directions },
    { "shift",          EQ, 1,  .list = completion_directions },
    { "shift",          EQ, 1,  .list = completion_focus_args },
    { "shift",          EQ, 2,  .list = completion_directions },
    { "set",            EQ, 1,  .function = complete_against_settings },
    { "split",          EQ, 1,  .list = completion_split_modes },
    { "split",          EQ, 2,  .list = completion_split_ratios },
    { "get",            EQ, 1,  .function = complete_against_settings },
    { "toggle",         EQ, 1,  .function = complete_against_settings },
    { "cycle_value",    EQ, 1,  .function = complete_against_settings },
    { "set_layout",     EQ, 1,  .list = g_layout_names },
    { "cycle_layout",   EQ, 1,  .list = completion_pm_one },
    { "cycle_layout",   GE, 2,  .list = g_layout_names },
    { "unrule",         EQ, 1,  .list = completion_unrule_args },
    { "use",            EQ, 1,  .function = complete_against_tags },
    { "use_index",      EQ, 1,  .list = completion_pm_one },
    { "use_index",      EQ, 2,  .list = completion_use_index_args },
    { 0 },
};

int call_command(int argc, char** argv, GString* output) {
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
        g_string_append_printf(output,
            "error: Command \"%s\" not found\n", argv[0]);
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

int call_command_no_output(int argc, char** argv) {
    GString* output = g_string_new("");
    int status = call_command(argc, argv, output);
    g_string_free(output, true);
    return status;
}

int list_commands(int argc, char** argv, GString* output)
{
    int i = 0;
    while (g_commands[i].cmd.standard != NULL) {
        g_string_append(output, g_commands[i].name);
        g_string_append(output, "\n");
        i++;
    }
    return 0;
}

void complete_against_list(char* needle, char** list, GString* output) {
    size_t len = strlen(needle);
    while (*list) {
        char* name = *list;
        if (!strncmp(needle, name, len)) {
            g_string_append(output, name);
            g_string_append(output, "\n");
        }
        list++;
    }
}

void complete_against_tags(int argc, char** argv, int pos, GString* output) {
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
            g_string_append(output, name);
            g_string_append(output, "\n");
        }
    }
}

void complete_negate(int argc, char** argv, int pos, GString* output) {
    if (pos <= 0) {
        return;
    }
    // Remove the ! from the argv
    (void)SHIFT(argc, argv);
    pos--;
    // Complete as normal
    complete_against_commands(argc, argv, pos, output);
}

struct wcd { /* window id completion data */
    char* needle;
    size_t needlelen;
    GString* output;
};

static void add_winid_completion(void* key, HSClient* client, struct wcd* data)
{
    char buf[100];
    GString* out = data->output;
    snprintf(buf, LENGTH(buf), "0x%lx\n", client->window);
    if (!strncmp(buf, data->needle, data->needlelen)) {
        g_string_append(out, buf);
    }
}

void complete_against_winids(int argc, char** argv, int pos, GString* output) {
    struct wcd data;
    if (pos >= argc) {
        data.needle = "";
    } else {
        data.needle = argv[pos];
    }
    data.needlelen = strlen(data.needle);
    data.output = output;
    clientlist_foreach((GHFunc)add_winid_completion, &data);
}

void complete_merge_tag(int argc, char** argv, int pos, GString* output) {
    char* first = (argc > 1) ? argv[1] : "";
    char* needle;
    if (pos >= argc) {
        needle = "";
    } else {
        needle = argv[pos];
    }
    size_t len = strlen(needle);
    for (int i = 0; i < g_tags->len; i++) {
        char* name = g_array_index(g_tags, HSTag*, i)->name->str;
        if (!strcmp(name, first)) {
            // merge target must not be equal to tag to remove
            continue;
        }
        if (!strncmp(needle, name, len)) {
            g_string_append(output, name);
            g_string_append(output, "\n");
        }
    }
}

void complete_against_settings(int argc, char** argv, int pos, GString* output)
{
    char* needle;
    if (pos >= argc) {
        needle = "";
    } else {
        needle = argv[pos];
    }
    size_t len = strlen(needle);
    bool is_toggle_command = !strcmp(argv[0], "toggle");
    // complete with setting name
    for (int i = 0; i < settings_count(); i++) {
        if (is_toggle_command && g_settings[i].type != HS_Int) {
            continue;
        }
        // only check the first len bytes
        if (!strncmp(needle, g_settings[i].name, len)) {
            g_string_append(output, g_settings[i].name);
            g_string_append(output, "\n");
        }
    }
}

void complete_against_keybinds(int argc, char** argv, int pos, GString* output) {
    char* needle;
    if (pos >= argc) {
        needle = "";
    } else {
        needle = argv[pos];
    }
    key_find_binds(needle, output);
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

int complete_command(int argc, char** argv, GString* output) {
    // usage: complete POSITION command to complete ...
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    // index must be between first and last arg of "command to complete ..."
    int position = CLAMP(atoi(argv[1]), 0, argc-2);
    (void)SHIFT(argc, argv);
    (void)SHIFT(argc, argv);
    return complete_against_commands(argc, argv, position, output);
}

void complete_against_keybind_command(int argc, char** argv, int position,
                                      GString* output) {
    if (argc <  2 || position < 2) {
        return;
    }
    complete_against_commands(argc - 2, argv + 2, position - 2, output);
}

int complete_against_commands(int argc, char** argv, int position,
                              GString* output) {
    // complete command
    if (position == 0) {
        char* str = (argc >= 1) ? argv[0] : "";
        size_t len = strlen(str);
        int i = 0;
        while (g_commands[i].cmd.standard != NULL) {
            // only check the first len bytes
            if (!strncmp(str, g_commands[i].name, len)) {
                g_string_append(output, g_commands[i].name);
                g_string_append(output, "\n");
            }
            i++;
        }
        return 0;
    }
    if (!parameter_expected(argc, argv, position)) {
        return HERBST_NO_PARAMETER_EXPECTED;
    }
    if (argc >= 1) {
        char* cmd_str = (argc >= 1) ? argv[0] : "";
        // complete parameters for commands
        for (int i = 0; i < LENGTH(g_completions); i++) {
            bool matches = false;
            switch (g_completions[i].relation) {
                case LE: matches = position <= g_completions[i].index; break;
                case EQ: matches = position == g_completions[i].index; break;
                case GE: matches = position >= g_completions[i].index; break;
            }
            if (!matches
                || !g_completions[i].command
                || strcmp(cmd_str, g_completions[i].command)) {
                continue;
            }
            char* needle = (position < argc) ? argv[position] : "";
            if (!needle) {
                needle = "";
            }
            // try to complete
            if (g_completions[i].function) {
                g_completions[i].function(argc, argv, position, output);
            }
            if (g_completions[i].list) {
                complete_against_list(needle, g_completions[i].list,
                                      output);
            }
        }
    }
    return 0;
}

static int strpcmp(const void* key, const void* val) {
    return strcmp((const char*) key, *(const char**)val);
}

static void complete_chain_helper(int argc, char** argv, int position,
                                  GString* output) {
    /* argv entries:
     * argv[0]      == the command separator
     * argv[1]      == an arbitrary command name
     * argv[2..]    == its arguments or a separator
     */
    if (position <= 0 || argc <= 1) {
        return;
    }
    char* separator = argv[0];
    (void)SHIFT(argc, argv);
    position--;

    /* find the next separator */
    size_t uargc = argc;
    char** next_sep = lfind(separator, argv, &uargc, sizeof(*argv), strpcmp);
    int next_sep_idx = next_sep - argv;

    if (!next_sep || next_sep_idx >= position) {
        /* try to complete at the desired position */
        char* needle = (position < argc) ? argv[position] : "";
        complete_against_commands(argc, argv, position, output);
        /* at least the command name is required
         * so don't complete at position 0 */
        if (position != 0 && 0 == strncmp(needle, separator, strlen(needle))) {
            g_string_append_printf(output, "%s\n", separator);
        }
    } else {
        /* remove arguments so that the next separator becomes argv[0] */
        position -= next_sep_idx;
        argc     -= next_sep_idx;
        argv     += next_sep_idx;
        complete_chain_helper(argc, argv, position, output);
    }
}

void complete_chain(int argc, char** argv, int position, GString* output) {
    if (position <= 1) {
        return;
    }
    /* remove the chain command name "chain" from the argv */
    (void)SHIFT(argc, argv);
    position--;

    /* do the actual work in the helper that always expects
     * {separator, firstcommand, ...}
     */
    complete_chain_helper(argc, argv, position, output);
}

bool first_parameter_is_tag(int argc, char** argv, int pos) {
    // only complete if first parameter is a valid tag
    if (argc >= 2 && find_tag(argv[1]) && pos == 2) {
        return true;
    } else {
        return false;
    }
}

bool first_parameter_is_flag(int argc, char** argv, int pos) {
    // only complete if first parameter is a flag like -i or -e
    if (argc >= 2 && argv[1][0] == '-' && pos == 2) {
        return true;
    } else {
        return false;
    }
}

bool keybind_parameter_expected(int argc, char** argv, int pos) {
    if (argc < 2 || pos < 2) {
        return true;
    }
    if (pos == 2) {
        // at least a command name always is expected
        return true;
    }
    return parameter_expected(argc - 2, argv + 2, pos - 2);
}

int command_chain(char* separator, bool (*condition)(int laststatus),
                  int argc, char** argv, GString* output) {
    size_t uargc = argc;
    printf("finding %s in %d elements\n", separator, argc);
    char** next_sep = lfind(separator, argv, &uargc, sizeof(*argv), strpcmp);
    int command_argc = next_sep ? (int)(next_sep - argv) : argc;
    int status = call_command(command_argc, argv, output);
    if (condition && false == condition(status)) {
        return status;
    }
    argc -= command_argc + 1;
    argv += command_argc + 1;
    if (argc <= 0) {
        return status;
    }
    return command_chain(separator, condition, argc, argv, output);
}

static bool int_is_zero(int x) {
    return x == 0;
}

static bool int_is_not_zero(int x) {
    return x != 0;
}

typedef struct {
    char* cmd;
    bool (*condition)(int);
} Cmd2Condition;

static Cmd2Condition g_cmd2condition[] = {
    { "and",    int_is_zero         },
    { "or",     int_is_not_zero     },
};

int command_chain_command(int argc, char** argv, GString* output) {
    Cmd2Condition* cmd;
    cmd = STATIC_TABLE_FIND_STR(Cmd2Condition, g_cmd2condition, cmd, argv[0]);
    (void)SHIFT(argc, argv);
    if (argc <= 1) {
        return HERBST_NEED_MORE_ARGS;
    }
    char* separator = argv[0];
    (void)SHIFT(argc, argv);
    bool (*condition)(int) = cmd ? cmd->condition : NULL;
    return command_chain(separator, condition, argc, argv, output);
}

int negate_command(int argc, char** argv, GString* output) {
    if (argc <= 1) {
        return HERBST_NEED_MORE_ARGS;
    }
    (void)SHIFT(argc, argv);
    int status = call_command(argc, argv, output);
    return (!status);
}

