#include "command.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "client.h"
#include "clientmanager.h"
#include "completion.h"
#include "glib-backports.h"
#include "ipc-protocol.h"
#include "key.h"
#include "layout.h"
#include "monitor.h"
#include "monitormanager.h"
#include "mouse.h"
#include "object.h"
#include "root.h"
#include "tag.h"
#include "utils.h"

// Quarantined inclusion to avoid polluting the global namespace:
namespace search_h {
    #include <search.h>
} // namespace search_h
using search_h::lfind;

using std::function;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;

extern char** environ;

// if the current completion needs shell quoting and other shell specific
// behaviour
static bool g_shell_quoting = false;

static const char* completion_directions[]    = { "left", "right", "down", "up",nullptr};
static const char* completion_focus_args[]    = { "-i", "-e", nullptr };
static const char* completion_flag_args[]     = { "on", "off", "true", "false", "toggle", nullptr };
static const char* completion_userattribute_types[] = { "int", "uint", "string", "bool", "color", nullptr };
static const char* completion_status[]        = { "status", nullptr };
static const char* completion_special_winids[]= { "urgent", "", nullptr };
static const char* completion_use_index_args[]= { "--skip-visible", nullptr };
static const char* completion_cycle_all_args[]= { "--skip-invisible", nullptr };
static const char* completion_pm_one[]= { "+1", "-1", nullptr };
static const char* completion_mouse_functions[]= { "move", "zoom", "resize", "call", nullptr };
static const char* completion_detect_monitors_args[] =
    { "-l", "--list", "--no-disjoin", /* TODO: "--keep-small", */ nullptr };
static const char* completion_split_modes[]= { "horizontal", "vertical", "left", "right", "top", "bottom", "explode", "auto", nullptr };
static const char* completion_split_ratios[]= {
    "0.1", "0.2", "0.3", "0.4", "0.5", "0.6", "0.7", "0.8", "0.9", nullptr };

static bool no_completion(int, char**, int) {
    return false;
}

static bool first_parameter_is_tag(int argc, char** argv, int pos);
static bool first_parameter_is_flag(int argc, char** argv, int pos);
static bool second_parameter_is_call(int argc, char** argv, int pos);
static bool parameter_expected_offset(int argc, char** argv, int pos, int offset);
static bool parameter_expected_offset_1(int argc, char** argv, int pos);
static bool parameter_expected_offset_2(int argc, char** argv, int pos);
static bool parameter_expected_offset_3(int argc, char** argv, int pos);

/* find out, if a command still expects a parameter at a certain index.
 * only if this returns true, than a completion will be searched.
 *
 * if no match is found, then it defaults to "command still expects a
 * parameter".
 */
struct {
    const char*   command; /* the first argument */
    int     min_index;  /* rule will only be considered */
                        /* if current pos >= min_index */
    bool    (*function)(int argc, char** argv, int pos);
} g_parameter_expected[] = {
    { "!",              2,  parameter_expected_offset_1 },
    { "try",            2,  parameter_expected_offset_1 },
    { "silent",         2,  parameter_expected_offset_1 },
    { "keybind",        2,  parameter_expected_offset_2 },
    { "mousebind",      3,  second_parameter_is_call },
    { "mousebind",      3,  parameter_expected_offset_3 },
    { "focus_nth",      2,  no_completion },
    { "close",          2,  no_completion },
    { "cycle",          2,  no_completion },
    { "cycle_all",      3,  no_completion },
    { "cycle_layout",   LAYOUT_COUNT+2, no_completion },
    { "set_layout",     2,  no_completion },
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
    { "cycle_monitor",  2,  no_completion },
    { "focus_monitor",  2,  no_completion },
    { "shift_to_monitor",2,  no_completion },
    { "add",            2,  no_completion },
    { "use",            2,  no_completion },
    { "use_index",      3,  no_completion },
    { "merge_tag",      3,  no_completion },
    { "rename",         3,  no_completion },
    { "move",           2,  no_completion },
    { "move_index",     3,  no_completion },
    { "lock_tag",       2,  no_completion },
    { "unlock_tag",     2,  no_completion },
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
    { "object_tree",    2,  no_completion },
    { "new_attr",       3,  no_completion },
    { "mktemp",         3,  parameter_expected_offset_3 },
    { "substitute",     3,  parameter_expected_offset_3 },
    { "getenv",         2,  no_completion },
    { "setenv",         3,  no_completion },
    { "unsetenv",       2,  no_completion },
};

enum IndexCompare {
    LE, /* lower equal */
    EQ, /* equal to */
    GE, /* greater equal */
};

/* list of completions, if a line matches, then it will be used, the order
 * does not matter */
struct {
    const char*   command;
    IndexCompare  relation; /* defines how the index matches */
    int     index;      /* which parameter to complete */
                        /* command name is index = 0 */
                        /* GE 0 matches any position */
                        /* LE 3 matches position from 0 to 3 */
    /* === various methods, how to complete === */
    /* completion by function */
    void (*function)(int argc, char** argv, int pos, Output output);
    /* completion by a list of strings */
    const char** list;
} g_completions[] = {
    /* name , relation, index,  completion method                   */
    { "add_monitor",    EQ, 2,  complete_against_tags, 0 },
    { "and",            GE, 1,  complete_chain, 0 },
    { "bring",          EQ, 1,  nullptr, completion_special_winids },
    { "bring",          EQ, 1,  complete_against_winids, 0 },
    { "close",          EQ, 1,  complete_against_winids, 0 },
    { "cycle",          EQ, 1,  nullptr, completion_pm_one },
    { "chain",          GE, 1,  complete_chain, 0 },
    { "cycle_all",      EQ, 1,  nullptr, completion_cycle_all_args },
    { "cycle_all",      EQ, 1,  nullptr, completion_pm_one },
    { "cycle_all",      EQ, 2,  nullptr, completion_pm_one },
    { "cycle_monitor",  EQ, 1,  nullptr, completion_pm_one },
    { "dump",           EQ, 1,  complete_against_tags, 0 },
    { "detect_monitors", GE, 1,  nullptr, completion_detect_monitors_args },
    { "floating",       EQ, 1,  complete_against_tags, 0 },
    { "floating",       EQ, 1,  nullptr, completion_flag_args },
    { "floating",       EQ, 1,  nullptr, completion_status },
    { "floating",       EQ, 2,  nullptr, completion_flag_args },
    { "floating",       EQ, 2,  nullptr, completion_status },
    { "focus",          EQ, 1,  nullptr, completion_directions },
    { "focus",          EQ, 1,  nullptr, completion_focus_args },
    { "focus",          EQ, 2,  nullptr, completion_directions },
    { "layout",         EQ, 1,  complete_against_tags, 0 },
    { "load",           EQ, 1,  complete_against_tags, 0 },
    { "merge_tag",      EQ, 1,  complete_against_tags, 0 },
    { "merge_tag",      EQ, 2,  complete_merge_tag, 0 },
    { "move",           EQ, 1,  complete_against_tags, 0 },
    { "move_index",     EQ, 2,  nullptr, completion_use_index_args },
    { "or",             GE, 1,  complete_chain, 0 },
    { "!",              GE, 1,  complete_against_commands_1, 0 },
    { "try",            GE, 1,  complete_against_commands_1, 0 },
    { "silent",         GE, 1,  complete_against_commands_1, 0 },
    { "keybind",        GE, 1,  complete_against_keybind_command, 0 },
    { "mousebind",      EQ, 1,  complete_against_mouse_combinations, 0 },
    { "mousebind",      EQ, 2,  nullptr, completion_mouse_functions },
    { "mousebind",      GE, 3,  complete_against_commands_3, 0 },
    { "rename",         EQ, 1,  complete_against_tags, 0 },
    { "raise",          EQ, 1,  nullptr, completion_special_winids },
    { "raise",          EQ, 1,  complete_against_winids, 0 },
    { "jumpto",         EQ, 1,  nullptr, completion_special_winids },
    { "jumpto",         EQ, 1,  complete_against_winids, 0 },
    { "resize",         EQ, 1,  nullptr, completion_directions },
    { "shift_edge",     EQ, 1,  nullptr, completion_directions },
    { "shift",          EQ, 1,  nullptr, completion_directions },
    { "shift",          EQ, 1,  nullptr, completion_focus_args },
    { "shift",          EQ, 2,  nullptr, completion_directions },
    { "split",          EQ, 1,  nullptr, completion_split_modes },
    { "split",          EQ, 2,  nullptr, completion_split_ratios },
    { "set_layout",     EQ, 1,  nullptr, g_layout_names },
    { "cycle_layout",   EQ, 1,  nullptr, completion_pm_one },
    { "cycle_layout",   GE, 2,  nullptr, g_layout_names },
    { "use",            EQ, 1,  complete_against_tags, 0 },
    { "use_index",      EQ, 1,  nullptr, completion_pm_one },
    { "use_index",      EQ, 2,  nullptr, completion_use_index_args },
    { "focus_monitor",  EQ, 1,  complete_against_monitors, 0 },
    { "shift_to_monitor",EQ, 1,  complete_against_monitors, 0 },
    { "lock_tag",       EQ, 1,  complete_against_monitors, 0 },
    { "unlock_tag",     EQ, 1,  complete_against_monitors, 0 },
    { "rename_monitor", EQ, 1,  complete_against_monitors, 0 },
    { "remove_monitor", EQ, 1,  complete_against_monitors, 0 },
    { "move_monitor",   EQ, 1,  complete_against_monitors, 0 },
    { "raise_monitor",  EQ, 1,  complete_against_monitors, 0 },
    { "name_monitor",   EQ, 1,  complete_against_monitors, 0 },
    { "monitor_rect",   EQ, 1,  complete_against_monitors, 0 },
    { "pad",            EQ, 1,  complete_against_monitors, 0 },
    { "list_padding",   EQ, 1,  complete_against_monitors, 0 },
    { "tag_status",     EQ, 1,  complete_against_monitors, 0 },
    { "setenv",         EQ, 1,  complete_against_env, 0 },
    { "getenv",         EQ, 1,  complete_against_env, 0 },
    { "unsetenv",       EQ, 1,  complete_against_env, 0 },
    { "compare",        EQ, 1,  complete_against_objects, 0 },
    { "compare",        EQ, 1,  complete_against_attributes, 0 },
    { "compare",        EQ, 2,  complete_against_comparators, 0 },
    { "compare",        EQ, 3,  complete_against_attribute_values, 0 },
    { "new_attr",       EQ, 1,  nullptr, completion_userattribute_types },
    { "new_attr",       EQ, 2,  complete_against_objects, 0 },
    { "new_attr",       EQ, 2,  complete_against_user_attr_prefix, 0 },
    { "mktemp",         EQ, 1,  nullptr, completion_userattribute_types },
    { "mktemp",         GE, 3,  complete_against_commands_3, 0 },
    { "mktemp",         GE, 4,  complete_against_arg_2, 0 },
    { "substitute",     EQ, 2,  complete_against_objects, 0 },
    { "substitute",     EQ, 2,  complete_against_attributes, 0 },
    { "substitute",     GE, 3,  complete_against_commands_3, 0 },
    { "substitute",     GE, 3,  complete_against_arg_1, 0 },
    { "sprintf",        GE, 3,  complete_sprintf, 0 },
};

// Implementation of CommandBinding

CommandBinding::CommandBinding(function<int(Output)> cmd)
    : command([cmd](Input, Output output) { return cmd(output); })
    , completion_([](Completion& c) { c.none(); })
{}

CommandBinding::CommandBinding(function<int()> cmd)
    : command([cmd](Input, Output) { return cmd(); })
    , completion_([](Completion& c) { c.none(); })
{}

// Nearly all of the following can go away, if all C-style command functions
// have been migrated to int(Input, Output).

/* Creates an ephemeral argv array from the given Input */
function <int(Input,Output)> CommandBinding::commandFromCFunc(
        function <int(int argc, char**argv, Output output)> func) {
    return [func](Input args, Output out) {
        /* Note that instead of copying the arguments, we point to their
         * original location here. This only works because Input stores its
         * payload in shared pointers and other references to them are held
         * until the command is finished.
         */
        shared_ptr<char*> argv(new char*[args.size() + 1],
                std::default_delete<char*[]>());

        // Most of the commands want a char**, not a const char**. Let's
        // hope, they don't actually modify it.
        argv.get()[0] = const_cast<char*>(args.command().c_str());
        auto elem = args.begin();
        for (size_t i = 0; i < args.size(); i++, elem++) {
            argv.get()[i+1] = const_cast<char*>(elem->c_str());
        }

        return func(args.size() + 1, argv.get(), out);
    };
}

CommandBinding::CommandBinding(int func(int argc, const char** argv, Output output))
    : command(commandFromCFunc([func](int argc, char** argv, Output out) {
                return func(argc, const_cast<const char**>(argv), out);
            }))
{}

CommandBinding::CommandBinding(int func(int argc, char** argv))
    : command(commandFromCFunc([func](int argc, char **argv, Output) {
                return func(argc, argv);
            }))
{}

CommandBinding::CommandBinding(int func(int argc, const char** argv))
    : command(commandFromCFunc([func](int argc, char** argv, Output) {
                return func(argc, const_cast<const char**>(argv));
            }))
{}


/** Complete the given list of arguments
 */
void CommandBinding::complete(Completion& completion) const {
    if ((bool) completion_) {
        completion_(completion);
    }
}

// Implementation of CommandTable
int CommandTable::callCommand(Input args, Output out) const {
    if (args.command().empty()) {
        // may happen if you call sprintf, but nothing afterwards
        return HERBST_NEED_MORE_ARGS;
    }

    const auto cmd = map.find(args.command());

    if (cmd == map.end()) {
        out << "error: Command \"" << args.command() << "\" not found\n";
        return HERBST_COMMAND_NOT_FOUND;
    }

    return cmd->second(args, out);
}

namespace Commands {
    shared_ptr<const CommandTable> command_table;
}

void Commands::initialize(unique_ptr<const CommandTable> commands) {
    if (!command_table) {
        command_table = move(commands);
    }
    // TODO What do we do in the 'already initialized' case?
}

int Commands::call(Input args, Output out) {
    if (!command_table) {
        return HERBST_COMMAND_NOT_FOUND;
    }
    return command_table->callCommand(args, out);
}

shared_ptr<const CommandTable> Commands::get() {
    if (!command_table) {
        throw std::logic_error("CommandTable not initialized, but get() called.");
    }
    return command_table;
}

void Commands::complete(Completion& completion) {
    // wrap around complete_against_commands
    // Once we have migrated all completions, we can implement command
    // completion directly with the minimal interface that Completion provides.
    // Then we can also unfriend this function from the Completion class.
    char** argv = new char*[completion.args_.size() + 1];
    argv[completion.args_.size()] = nullptr;
    for (size_t i = 0; i < completion.args_.size(); i++) {
        argv[i] = const_cast<char*>((completion.args_.begin() + i)->c_str());
    }
    int res = complete_against_commands(completion.args_.size(),
                                        argv,
                                        completion.index_,
                                        completion.output_);
    delete[] argv;
    if (res == HERBST_NO_PARAMETER_EXPECTED) {
        completion.noParameterExpected();
    } else if (res != 0) {
        completion.invalidArguments();
    }
}


// Old C-ish interface to commands:

int call_command(int argc, char** argv, Output output) {
    if (argc < 1)
        return HERBST_COMMAND_NOT_FOUND;

    string cmd(argv[0]);
    vector<string> args;
    for (int i = 1; i < argc; i++) {
        args.push_back(argv[i]);
    }

    return Commands::call(Input(cmd, args), output);
}

int call_command_no_output(int argc, char** argv) {
    std::ostringstream output;
    return call_command(argc, argv, output);
}

int list_commands(Output output)
{
    for (auto cmd : *Commands::get()) {
        output << cmd.first << std::endl;
    }
    return 0;
}

static void try_complete_suffix(const char* needle, const char* to_check, const char* suffix,
                                const char* prefix, Output output)
{
    bool matches = !needle;
    if (!matches) {
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
        char* escaped = nullptr;
        if (g_shell_quoting) {
            escaped = posix_sh_escape(to_check);
        }
        char* prefix_escaped = nullptr;
        if (prefix) {
            if (g_shell_quoting) {
                prefix_escaped = posix_sh_escape(prefix);
            }
            output << (prefix_escaped ? prefix_escaped : prefix);
        }
        output << (escaped ? escaped : to_check);
        free(escaped);
        output << suffix;
    }
}

void try_complete(const char* needle, std::string to_check, Output output) {
    try_complete(needle, to_check.c_str(), output);
}

void try_complete(const char* needle, const char* to_check, Output output) {
    const char* suffix = g_shell_quoting ? " \n" : "\n";
    try_complete_suffix(needle, to_check, suffix, nullptr, output);
}

void try_complete_prefix(const char* needle, const char* to_check,
                         const char* prefix, Output output) {
    const char* suffix = g_shell_quoting ? " \n" : "\n";
    try_complete_suffix(needle, to_check, suffix, prefix, output);
}

void try_complete_partial(const char* needle, const char* to_check, Output output) {
    try_complete_suffix(needle, to_check, "\n", nullptr, output);
}

void try_complete_prefix_partial(const char* needle, const char* to_check,
                                 const char* prefix, Output output) {
    try_complete_suffix(needle, to_check, "\n", prefix, output);
}
void try_complete_prefix_partial(const std::string& needle, const std::string& to_check,
                                 const std::string& prefix, Output output) {
    try_complete_suffix(needle.c_str(), to_check.c_str(), "\n", prefix.c_str(), output);
}

void complete_against_list(const char* needle, const char** list, Output output) {
    while (*list) {
        const char* name = *list;
        try_complete(needle, name, output);
        list++;
    }
}

void complete_against_tags(int argc, char** argv, int pos, Output output) {
    const char* needle;
    if (pos >= argc) {
        needle = "";
    } else {
        needle = argv[pos];
    }
    for (int i = 0; i < tag_get_count(); i++) {
        const char* name = get_tag_by_index(i)->name->c_str();
        try_complete(needle, name, output);
    }
}

void complete_against_monitors(int argc, char** argv, int pos, Output output) {
    const char* needle;
    if (pos >= argc) {
        needle = "";
    } else {
        needle = argv[pos];
    }
    // complete against relative indices
    try_complete(needle, "-1", output);
    try_complete(needle, "+1", output);
    try_complete(needle, "+0", output);
    for (auto m : *g_monitors) {
        // complete against the absolute index
        try_complete(needle, to_string(m->index()), output);
        // complete against the name
        if (m->name != "") {
            try_complete(needle, m->name->c_str(), output);
        }
    }
}

void complete_against_objects(int argc, char** argv, int pos, Output output) {
    // Remove command name
    (void)SHIFT(argc,argv);
    pos--;

    std::pair<ArgList,std::string> p = Object::splitPath((pos < argc) ? argv[pos] : "");
    auto needle = p.second;
    Object* o = Root::get()->child(p.first);
    if (!o) {
        return;
    }
    auto prefix = p.first.join();
    if (!prefix.empty()) prefix += ".";
    for (auto a : o->children()) {
        try_complete_prefix_partial(needle.c_str(), (a.first + ".").c_str(), prefix.c_str(), output);
    }
    return;
}


void complete_against_attributes_helper(int argc, char** argv, int pos,
                                        Output output, bool user_only) {
    // Remove command name
    (void)SHIFT(argc,argv);
    pos--;

    std::pair<ArgList,std::string> p = Object::splitPath((pos < argc) ? argv[pos] : "");
    auto needle = p.second;
    Object* o = Root::get()->child(p.first);
    if (!o) {
        return;
    }
    auto prefix = p.first.join();
    if (!prefix.empty()) prefix += ".";
    for (auto a : o->attributes()) {
        try_complete_prefix(needle.c_str(), a.first.c_str(), prefix.c_str(), output);
    }
    return;
}


void complete_against_attributes(int argc, char** argv, int pos, Output output) {
    complete_against_attributes_helper(argc, argv, pos, output, false);
}

void complete_against_user_attributes(int argc, char** argv, int pos, Output output) {
    complete_against_attributes_helper(argc, argv, pos, output, true);
}


void complete_against_user_attr_prefix(int argc, char** argv, int position,
                                      Output output) {
    // TODO
    /*
    const char* path = (position < argc) ? argv[position] : "";
    const char* unparsable;

    GString* prefix = g_string_new(path);

    if (prefix->len > 0
        && prefix->str[prefix->len - 1] != OBJECT_PATH_SEPARATOR) {
        g_string_append_c(prefix, OBJECT_PATH_SEPARATOR);
    }
    try_complete_prefix_partial(unparsable, USER_ATTRIBUTE_PREFIX,
                                prefix->str, output);
    */
}

void complete_against_attribute_values(int argc, char** argv, int pos, Output output) {
    // TODO
    /*
    const char* needle = (pos < argc) ? argv[pos] : "";
    const char* path =  (1 < argc) ? argv[1] : "";
    std::ostringstream path_error;
    HSAttribute* attr = hsattribute_parse_path_verbose(path, path_error);
    if (attr) {
        switch (attr->type) {
            case HSATTR_TYPE_BOOL:
                complete_against_list(needle, completion_flag_args, output);
            default:
                // no suitable completion
                break;
        }
    }
    */
}

void complete_against_comparators(int argc, char** argv, int pos, Output output) {
    // TODO
    /*
    const char* needle = (pos < argc) ? argv[pos] : "";
    const char* path =  (1 < argc) ? argv[1] : "";
    std::ostringstream void_output;
    HSAttribute* attr = hsattribute_parse_path_verbose(path, void_output);
    const char* equals[] = { "=", "!=", nullptr };
    const char* order[] = { "le", "lt", "ge", "gt", nullptr };
    if (attr) {
        switch (attr->type) {
            case HSATTR_TYPE_INT:
            case HSATTR_TYPE_UINT:
            case HSATTR_TYPE_CUSTOM_INT:
                complete_against_list(needle, order, output);
            default:
                complete_against_list(needle, equals, output);
                break;
        }
    }
    */
}

void complete_against_winids(int argc, char** argv, int pos, Output output) {
    const char* needle;
    if (pos >= argc) {
        needle = "";
    } else {
        needle = argv[pos];
    }
    for (auto c : Root::get()->clients()->clients()) {
        char buf[100];
        snprintf(buf, LENGTH(buf), "0x%lx", c.second->window_);
        try_complete(needle, buf, output);
    }
}

void complete_merge_tag(int argc, char** argv, int pos, Output output) {
    const char* first = (argc > 1) ? argv[1] : "";
    const char* needle;
    if (pos >= argc) {
        needle = "";
    } else {
        needle = argv[pos];
    }
    for (int i = 0; i < tag_get_count(); i++) {
        const char* name = get_tag_by_index(i)->name->c_str();
        if (!strcmp(name, first)) {
            // merge target must not be equal to tag to remove
            continue;
        }
        try_complete(needle, name, output);
    }
}

static bool parameter_expected(int argc, char** argv, int pos) {
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

int complete_command(int argc, char** argv, Output output) {
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
                                      Output output) {
    if (argc <  1 || position < 1) {
        return;
    }
    if (position == 1) {
        // complete the keycombination
        const char* needle = (position < argc) ? argv[position] : "";
        const char* lasttok = strlasttoken(needle, KEY_COMBI_SEPARATORS);
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

void complete_against_mouse_combinations(int argc, char** argv, int position,
                                         Output output)
{
    if (argc < 1 || position < 1) {
        return;
    }
    // complete the mouse combination
    const char* needle = (position < argc) ? argv[position] : "";
    const char* lasttok = strlasttoken(needle, KEY_COMBI_SEPARATORS);
    char* prefix = g_strdup(needle);
    prefix[lasttok - needle] = '\0';
    char separator = KEY_COMBI_SEPARATORS[0];
    if (lasttok != needle) {
        // if there is a suffix, then the already used separator is before
        // the start of the last token
        separator = lasttok[-1];
    }
    complete_against_modifiers(lasttok, separator, prefix, output);
    complete_against_mouse_buttons(lasttok, prefix, output);
    g_free(prefix);
}

void complete_against_env(int argc, char** argv, int position,
                          Output output) {
    GString* curname = g_string_sized_new(30);
    const char* needle = (position < argc) ? argv[position] : "";
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

void complete_against_commands_1(int argc, char** argv, int position,
                                      Output output) {
    complete_against_commands(argc - 1, argv + 1, position - 1, output);
}

void complete_against_commands_3(int argc, char** argv, int position,
                                      Output output) {
    complete_against_commands(argc - 3, argv + 3, position - 3, output);
}

void complete_against_arg_1(int argc, char** argv, int position,
                            Output output)
{
    if (argc > 2 && position > 2) {
        const char* needle = (position < argc) ? argv[position] : "";
        try_complete(needle, argv[1], output);
    }
}

void complete_against_arg_2(int argc, char** argv, int position,
                            Output output)
{
    if (argc > 3 && position > 3) {
        const char* needle = (position < argc) ? argv[position] : "";
        try_complete(needle, argv[2], output);
    }
}


int complete_against_commands(int argc, char** argv, int position,
                              Output output) {
    // complete command
    if (position == 0) {
        char* str = (argc >= 1) ? argv[0] : nullptr;
        for (auto cmd : *Commands::get()) {
            // only check the first len bytes
            try_complete(str, cmd.first.c_str(), output);
        }
        return 0;
    }
    // try to get completion from the command binding
    std::string commandName = argv[0];
    auto commandTable = Commands::get();
    auto it = commandTable->find(commandName);
    if (it == commandTable->end()) {
        return HERBST_INVALID_ARGUMENT;
    }
    if (it->second.hasCompletion()) {
        vector<string> arguments;
        for (int i = 1; i < argc; i++) {
            arguments.push_back(argv[i]);
        }
        // the new completion context has the command name removed
        Completion completion(arguments, position - 1, g_shell_quoting, output);
        it->second.complete(completion);
        return completion.noParameterExpected() ?
            HERBST_NO_PARAMETER_EXPECTED
            : 0;
    }
    if (!parameter_expected(argc, argv, position)) {
        return HERBST_NO_PARAMETER_EXPECTED;
    }
    if (argc >= 1) {
        const char* cmd_str = (argc >= 1) ? argv[0] : "";
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
            const char* needle = (position < argc) ? argv[position] : "";
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
                                  Output output) {
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
    char** next_sep = (char**)lfind(separator, argv, &uargc, sizeof(*argv), strpcmp);
    int next_sep_idx = next_sep - argv;

    if (!next_sep || next_sep_idx >= position) {
        /* try to complete at the desired position */
        const char* needle = (position < argc) ? argv[position] : "";
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

void complete_chain(int argc, char** argv, int position, Output output) {
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

void complete_sprintf(int argc, char** argv, int position, Output output) {
    const char* needle = (position < argc) ? argv[position] : "";
    int paramcount = 0;
    char* format = argv[2];
    for (int i = 0; format[i]; i++) {
        if (format[i] == '%') {
            i++; // look at the char after '%'
            if (format[i] != '%' && format[i] != '\0') {
                paramcount++;
            }
        }
    }
    char* identifier = argv[1];
    if (position < 3 + paramcount) {
        // complete attributes
        complete_against_objects(argc, argv, position, output);
        complete_against_attributes(argc, argv, position, output);
    } else {
        try_complete(needle, identifier, output);
        int delta = 3 + paramcount;
        complete_against_commands(argc - delta, argv + delta,
                                  position - delta, output);
    }
}

static bool first_parameter_is_tag(int argc, char** argv, int pos) {
    // only complete if first parameter is a valid tag
    if (argc >= 2 && find_tag(argv[1]) && pos == 2) {
        return true;
    } else {
        return false;
    }
}

static bool first_parameter_is_flag(int argc, char** argv, int pos) {
    // only complete if first parameter is a flag like -i or -e
    if (argc >= 2 && argv[1][0] == '-' && pos == 2) {
        return true;
    } else {
        return false;
    }
}

static bool second_parameter_is_call(int argc, char** argv, int pos) {
    if (argc >= 3 && !strcmp(argv[2], "call")) {
        return true;
    } else {
        return false;
    }
}

static bool parameter_expected_offset(int argc, char** argv, int pos, int offset) {
    if (argc < offset || pos < offset) {
        return true;
    }
    if (pos == offset) {
        // at least a command name always is expected
        return true;
    }
    return parameter_expected(argc - offset, argv + offset, pos - offset);
}

static bool parameter_expected_offset_1(int argc, char** argv, int pos) {
    return parameter_expected_offset(argc,argv, pos, 1);
}

static bool parameter_expected_offset_2(int argc, char** argv, int pos) {
    return parameter_expected_offset(argc,argv, pos, 2);
}

static bool parameter_expected_offset_3(int argc, char** argv, int pos) {
    return parameter_expected_offset(argc,argv, pos, 3);
}


int command_chain(char* separator, bool (*condition)(int laststatus),
                  int argc, char** argv, Output output) {
    size_t uargc = argc;
    char** next_sep = (char**)lfind(separator, argv, &uargc, sizeof(*argv), strpcmp);
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
    const char* cmd;
    bool (*condition)(int);
} Cmd2Condition;

static Cmd2Condition g_cmd2condition[] = {
    { "and",    int_is_zero         },
    { "or",     int_is_not_zero     },
};

int command_chain_command(int argc, char** argv, Output output) {
    Cmd2Condition* cmd;
    cmd = STATIC_TABLE_FIND_STR(Cmd2Condition, g_cmd2condition, cmd, argv[0]);
    (void)SHIFT(argc, argv);
    if (argc <= 1) {
        return HERBST_NEED_MORE_ARGS;
    }
    char* separator = argv[0];
    (void)SHIFT(argc, argv);
    bool (*condition)(int) = cmd ? cmd->condition : nullptr;
    return command_chain(separator, condition, argc, argv, output);
}

int negate_command(int argc, char** argv, Output output) {
    if (argc <= 1) {
        return HERBST_NEED_MORE_ARGS;
    }
    (void)SHIFT(argc, argv);
    int status = call_command(argc, argv, output);
    return (!status);
}

