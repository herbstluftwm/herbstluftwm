#include "command.h"

#include <cstdio>
#include <cstring>
#include <sstream>

#include "client.h"
#include "completion.h"
#include "hlwmcommon.h"
#include "ipc-protocol.h"
#include "monitor.h"
#include "monitormanager.h"
#include "root.h"
#include "tag.h"
#include "utils.h"

using std::endl;
using std::function;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;

static void try_complete(const char* needle, const char* to_check, Output output);

static int complete_against_commands(int argc, char** argv, int position, Output output);

// if the current completion needs shell quoting and other shell specific
// behaviour
static bool g_shell_quoting = false;

static const char* completion_pm_one[]= { "+1", "-1", nullptr };
static const char* completion_split_modes[]= { "horizontal", "vertical", "left", "right", "top", "bottom", "explode", "auto", nullptr };
static const char* completion_split_ratios[]= {
    "0.1", "0.2", "0.3", "0.4", "0.5", "0.6", "0.7", "0.8", "0.9", nullptr };

static bool no_completion(int, char**, int) {
    return false;
}

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
    { "cycle",          2,  no_completion },
    { "split",          4,  no_completion },
    { "add",            2,  no_completion },
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
    { "cycle",          EQ, 1,  nullptr, completion_pm_one },
    { "split",          EQ, 1,  nullptr, completion_split_modes },
    { "split",          EQ, 2,  nullptr, completion_split_ratios },
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
        out << "error: Command \"" << args.command() << "\" not found" << endl;
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

bool Commands::commandExists(const string& commandName)
{
    if (!command_table) {
        return false;
    }
    return command_table->find(commandName) != command_table->end();
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
        completion.none();
    } else if (res != 0) {
        completion.invalidArguments();
    }
}

int list_commands(Output output)
{
    for (auto cmd : *Commands::get()) {
        output << cmd.first << endl;
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
        if (prefix) {
            if (g_shell_quoting) {
                output << posix_sh_escape(prefix);
            } else {
                output << prefix;
            }
        }
        if (g_shell_quoting) {
            output << posix_sh_escape(to_check);
        } else {
            output << to_check;
        }
        output << suffix;
    }
}

void try_complete(const char* needle, const char* to_check, Output output) {
    const char* suffix = g_shell_quoting ? " \n" : "\n";
    try_complete_suffix(needle, to_check, suffix, nullptr, output);
}

static void complete_against_list(const char* needle, const char** list, Output output) {
    while (*list) {
        const char* name = *list;
        try_complete(needle, name, output);
        list++;
    }
}

static bool parameter_expected(int argc, char** argv, int pos) {
    if (pos <= 0 || argc < 1) {
        /* no parameter if there is no command */
        return false;
    }
    for (size_t i = 0; i < LENGTH(g_parameter_expected)
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
    string commandName = argv[0];
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
        for (size_t i = 0; i < LENGTH(g_completions); i++) {
            bool matches = false;
            switch (g_completions[i].relation) {
                case LE: matches = position <= g_completions[i].index; break;
                case EQ: matches = position == g_completions[i].index; break;
                case GE: matches = position >= g_completions[i].index; break;
            }
            if (!matches
                || !g_completions[i].command
                || strcmp(cmd_str, g_completions[i].command) != 0) {
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
