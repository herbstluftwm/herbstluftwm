/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
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
#include "monitor.h"
#include "rules.h"
#include "object.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <search.h>
#include <unistd.h>

extern char** environ;

// if the current completion needs shell quoting and other shell specific
// behaviour
static bool g_shell_quoting = false;

static char* completion_directions[]    = { "left", "right", "down", "up",NULL};
static char* completion_focus_args[]    = { "-i", "-e", NULL };
static char* completion_unrule_args[]   = { "-F", "--all", NULL };
static char* completion_keyunbind_args[]= { "-F", "--all", NULL };
static char* completion_flag_args[]     = { "on", "off", "true", "false", "toggle", NULL };
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
static bool parameter_expected_offset(int argc, char** argv, int pos, int offset);
static bool parameter_expected_offset_2(int argc, char** argv, int pos);
static bool parameter_expected_offset_3(int argc, char** argv, int pos);

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
    { "keybind",        2,  parameter_expected_offset_2 },
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
    { "use_previous",   1,  no_completion },
    { "merge_tag",      3,  no_completion },
    { "rename",         3,  no_completion },
    { "move",           2,  no_completion },
    { "move_index",     3,  no_completion },
    { "lock_tag",       2,  no_completion },
    { "unlock_tag",     2,  no_completion },
    { "detect_monitors",1,  no_completion },
    { "add_monitor",    7,  no_completion },
    { "rename_monitor", 3,  no_completion },
    { "remove_monitor", 2,  no_completion },
    { "move_monitor",   7,  no_completion },
    { "raise_monitor",  2,  no_completion },
    { "stack",          2,  no_completion },
    { "monitor_rect",   3,  no_completion },
    { "pad",            6,  no_completion },
    { "list_padding",   2,  no_completion },
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
    { "ls",             2,  no_completion },
    { "object_tree",    2,  no_completion },
    { "get_attribute",  2,  no_completion },
    { "set_attribute",  3,  no_completion },
    { "substitute",     3,  parameter_expected_offset_3 },
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
    { "keybind",        GE, 1,  .function = complete_against_keybind_command },
    { "keyunbind",      EQ, 1,  .list = completion_keyunbind_args },
    { "keyunbind",      EQ, 1,  .function = complete_against_keybinds },
    { "rename",         EQ, 1,  .function = complete_against_tags },
    { "raise",          EQ, 1,  .list = completion_special_winids },
    { "raise",          EQ, 1,  .function = complete_against_winids },
    { "jumpto",         EQ, 1,  .list = completion_special_winids },
    { "jumpto",         EQ, 1,  .function = complete_against_winids },
    { "resize",         EQ, 1,  .list = completion_directions },
    { "rule",           GE, 1,  .function = rule_complete },
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
    { "focus_monitor",  EQ, 1,  .function = complete_against_monitors },
    { "lock_tag",       EQ, 1,  .function = complete_against_monitors },
    { "unlock_tag",     EQ, 1,  .function = complete_against_monitors },
    { "rename_monitor", EQ, 1,  .function = complete_against_monitors },
    { "remove_monitor", EQ, 1,  .function = complete_against_monitors },
    { "move_monitor",   EQ, 1,  .function = complete_against_monitors },
    { "raise_monitor",  EQ, 1,  .function = complete_against_monitors },
    { "name_monitor",   EQ, 1,  .function = complete_against_monitors },
    { "monitor_rect",   EQ, 1,  .function = complete_against_monitors },
    { "pad",            EQ, 1,  .function = complete_against_monitors },
    { "list_padding",   EQ, 1,  .function = complete_against_monitors },
    { "tag_status",     EQ, 1,  .function = complete_against_monitors },
    { "setenv",         EQ, 1,  .function = complete_against_env },
    { "getenv",         EQ, 1,  .function = complete_against_env },
    { "unsetenv",       EQ, 1,  .function = complete_against_env },
    { "ls",             EQ, 1,  .function = complete_against_objects },
    { "object_tree",    EQ, 1,  .function = complete_against_objects },
    { "get_attribute",  EQ, 1,  .function = complete_against_objects },
    { "get_attribute",  EQ, 1,  .function = complete_against_attributes },
    { "set_attribute",  EQ, 1,  .function = complete_against_objects },
    { "set_attribute",  EQ, 1,  .function = complete_against_attributes },
    { "set_attribute",  EQ, 2,  .function = complete_against_attribute_values },
    { "substitute",     EQ, 2,  .function = complete_against_objects },
    { "substitute",     EQ, 2,  .function = complete_against_attributes },
    { "substitute",     GE, 3,  .function = complete_against_commands_3 },
    { "substitute",     GE, 3,  .function = complete_against_arg_1 },
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

static void try_complete_suffix(char* needle, char* to_check, char* suffix,
                                char* prefix, GString* output)
{
    bool matches = (needle == NULL);
    if (matches == false) {
        matches = true; // set it to true if the loop successfully runs
        // find the first difference between needle and to_check
        for (int i = 0; true ; i++) {
            // check if needle is a prefix of to_check
            if (!needle[i]) {
                break;
            }
            // if the needle is longer than to_check, then needle isn't a
            // correct prefix of to_check
            if (!to_check[i]) {
                matches = false;
                break;
            }
            // only proceed if they are identical
            if (to_check[i] != needle[i]) {
                matches = false;
                break;
            }
        }
    }
    if (matches) {
        char* escaped = NULL;
        if (g_shell_quoting) {
            escaped = posix_sh_escape(to_check);
        }
        char* prefix_escaped = NULL;
        if (prefix) {
            if (g_shell_quoting) {
                prefix_escaped = posix_sh_escape(prefix);
            }
            g_string_append(output, prefix_escaped ? prefix_escaped : prefix);
        }
        g_string_append(output, escaped ? escaped : to_check);
        free(escaped);
        g_string_append(output, suffix);
    }
}

void try_complete(char* needle, char* to_check, GString* output) {
    char* suffix = g_shell_quoting ? " \n" : "\n";
    try_complete_suffix(needle, to_check, suffix, NULL, output);
}

void try_complete_prefix(char* needle, char* to_check,
                         char* prefix, GString* output) {
    char* suffix = g_shell_quoting ? " \n" : "\n";
    try_complete_suffix(needle, to_check, suffix, prefix, output);
}

void try_complete_partial(char* needle, char* to_check, GString* output) {
    try_complete_suffix(needle, to_check, "\n", NULL, output);
}

void try_complete_prefix_partial(char* needle, char* to_check,
                                 char* prefix, GString* output) {
    try_complete_suffix(needle, to_check, "\n", prefix, output);
}

void complete_against_list(char* needle, char** list, GString* output) {
    while (*list) {
        char* name = *list;
        try_complete(needle, name, output);
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
    for (int i = 0; i < g_tags->len; i++) {
        char* name = g_array_index(g_tags, HSTag*, i)->name->str;
        try_complete(needle, name, output);
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

void complete_against_monitors(int argc, char** argv, int pos, GString* output) {
    char* needle;
    if (pos >= argc) {
        needle = "";
    } else {
        needle = argv[pos];
    }
    // complete against relative indices
    try_complete(needle, "-1", output);
    try_complete(needle, "+1", output);
    GString* index_str = g_string_sized_new(10);
    for (int i = 0; i < monitor_count(); i++) {
        // complete against the absolute index
        g_string_printf(index_str, "%d", i);
        try_complete(needle, index_str->str, output);
        // complete against the name
        GString* name = monitor_with_index(i)->name;
        if (name != NULL) {
            try_complete(needle, name->str, output);
        }
    }
    g_string_free(index_str, true);
}

void complete_against_objects(int argc, char** argv, int pos, GString* output) {
    // Remove command name
    (void)SHIFT(argc,argv);
    pos--;
    char* needle = (pos < argc) ? argv[pos] : "";
    char* suffix;
    char* prefix = g_new(char, strlen(needle)+2);
    HSObject* obj = hsobject_parse_path(needle, &suffix);
    strncpy(prefix, needle, suffix-needle);
    if (suffix != needle && prefix[suffix - needle - 1] != OBJECT_PATH_SEPARATOR) {
        prefix[suffix - needle] = OBJECT_PATH_SEPARATOR;
        prefix[suffix - needle + 1] = '\0';
    } else {
        prefix[suffix - needle] = '\0';
    }
    hsobject_complete_children(obj, suffix, prefix, output);
    g_free(prefix);
}

void complete_against_attributes(int argc, char** argv, int pos, GString* output) {
    // Remove command name
    (void)SHIFT(argc,argv);
    pos--;
    char* needle = (pos < argc) ? argv[pos] : "";
    char* unparsable;
    HSObject* obj = hsobject_parse_path(needle, &unparsable);
    if (obj && strchr(unparsable, OBJECT_PATH_SEPARATOR) == NULL) {
        GString* prefix = g_string_new(needle);
        g_string_truncate(prefix, unparsable - needle);
        if (prefix->len >= 1) {
            char last = prefix->str[prefix->len - 1];
            if (last != OBJECT_PATH_SEPARATOR) {
                g_string_append_c(prefix, OBJECT_PATH_SEPARATOR);
            }
        }
        hsobject_complete_attributes(obj, unparsable, prefix->str, output);
        g_string_free(prefix, true);
    }
}

void complete_against_attribute_values(int argc, char** argv, int pos, GString* output) {
    char* needle = (pos < argc) ? argv[pos] : "";
    char* path =  (1 < argc) ? argv[1] : "";
    GString* path_error = g_string_new("");
    HSAttribute* attr = hsattribute_parse_path_verbose(path, path_error);
    g_string_free(path_error, true);
    if (attr && attr->on_change != ATTR_READ_ONLY) {
        switch (attr->type) {
            case HSATTR_TYPE_BOOL:
                complete_against_list(needle, completion_flag_args, output);
            default:
                // no suitable completion
                break;
        }
    }
}

struct wcd { /* window id completion data */
    char* needle;
    GString* output;
};

static void add_winid_completion(void* key, HSClient* client, struct wcd* data)
{
    char buf[100];
    snprintf(buf, LENGTH(buf), "0x%lx", client->window);
    try_complete(data->needle, buf, data->output);

}

void complete_against_winids(int argc, char** argv, int pos, GString* output) {
    struct wcd data;
    if (pos >= argc) {
        data.needle = "";
    } else {
        data.needle = argv[pos];
    }
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
    for (int i = 0; i < g_tags->len; i++) {
        char* name = g_array_index(g_tags, HSTag*, i)->name->str;
        if (!strcmp(name, first)) {
            // merge target must not be equal to tag to remove
            continue;
        }
        try_complete(needle, name, output);
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
    bool is_toggle_command = !strcmp(argv[0], "toggle");
    // complete with setting name
    for (int i = 0; i < settings_count(); i++) {
        if (is_toggle_command && g_settings[i].type != HS_Int) {
            continue;
        }
        try_complete(needle, g_settings[i].name, output);
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
    char* cmdname = argv[0];
    g_shell_quoting = !strcmp(cmdname, "complete_shell");
    // index must be between first and last arg of "command to complete ..."
    int position = CLAMP(atoi(argv[1]), 0, argc-2);
    (void)SHIFT(argc, argv);
    (void)SHIFT(argc, argv);
    if (g_shell_quoting) {
        for (int i = 0; i < argc; i++) {
            posix_sh_compress_inplace(argv[i]);
        }
    }
    return complete_against_commands(argc, argv, position, output);
}

void complete_against_keybind_command(int argc, char** argv, int position,
                                      GString* output) {
    if (argc <  1 || position < 1) {
        return;
    }
    if (position == 1) {
        // complete the keycombination
        char* needle = (position < argc) ? argv[position] : "";
        char* lasttok = strlasttoken(needle, KEY_COMBI_SEPARATORS);
        char* prefix = g_strdup(needle);
        prefix[lasttok - needle] = '\0';
        char separator = KEY_COMBI_SEPARATORS[0];
        if (lasttok != needle) {
            // if there is a suffix, then the already used separator is before
            // the start of the last token
            separator = lasttok[-1];
        }
        complete_against_modifiers(lasttok, separator, prefix, output);
        complete_against_keysyms(lasttok, prefix, output);
        g_free(prefix);
    } else if (position >= 2 && argc >= 2) {
        // complete the command
        complete_against_commands(argc - 2, argv + 2, position - 2, output);
    }
}

void complete_against_env(int argc, char** argv, int position,
                          GString* output) {
    GString* curname = g_string_sized_new(30);
    char* needle = (position < argc) ? argv[position] : "";
    for (char** env = environ; *env; ++env) {
        g_string_assign(curname, *env);
        char* name_end = strchr(*env, '=');
        if (!name_end) {
            continue;
        }
        g_string_truncate(curname, name_end - *env);
        try_complete(needle, curname->str, output);
    }
    g_string_free(curname, true);
}

void complete_against_commands_3(int argc, char** argv, int position,
                                      GString* output) {
    complete_against_commands(argc - 3, argv + 3, position - 3, output);
}

void complete_against_arg_1(int argc, char** argv, int position,
                            GString* output)
{
    if (argc > 2 && position > 2) {
        char* needle = (position < argc) ? argv[position] : "";
        try_complete(needle, argv[1], output);
    }
}

int complete_against_commands(int argc, char** argv, int position,
                              GString* output) {
    // complete command
    if (position == 0) {
        char* str = (argc >= 1) ? argv[0] : NULL;
        for (int i = 0; g_commands[i].cmd.standard != NULL; i++) {
            // only check the first len bytes
            try_complete(str, g_commands[i].name, output);
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
        if (position != 0) {
            try_complete(needle, separator, output);
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

bool parameter_expected_offset(int argc, char** argv, int pos, int offset) {
    if (argc < offset || pos < offset) {
        return true;
    }
    if (pos == offset) {
        // at least a command name always is expected
        return true;
    }
    return parameter_expected(argc - offset, argv + offset, pos - offset);
}

bool parameter_expected_offset_2(int argc, char** argv, int pos) {
    return parameter_expected_offset(argc,argv, pos, 2);
}

bool parameter_expected_offset_3(int argc, char** argv, int pos) {
    return parameter_expected_offset(argc,argv, pos, 3);
}


int command_chain(char* separator, bool (*condition)(int laststatus),
                  int argc, char** argv, GString* output) {
    size_t uargc = argc;
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

