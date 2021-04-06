#include "command.h"

#include <cstdio>
#include <cstring>
#include <sstream>

#include "argparse.h"
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

CommandBinding::CommandBinding(int func(int argc, char** argv))
    : command(commandFromCFunc([func](int argc, char **argv, Output) {
                return func(argc, argv);
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
    auto commandTable = Commands::get();
    if (completion == 0) {
        for (const auto& cmd : *commandTable) {
            completion.full(cmd.first);
        }
    } else {
        auto it = commandTable->find(completion[0]);
        if (it == commandTable->end()) {
            completion.invalidArguments();
        } else if (it->second.hasCompletion()) {
            // get new completion context with command name omitted.
            Completion shifted = completion.shifted(1);
            it->second.complete(shifted);
            // TODO: call mergeResultsFrom(), once #1274 is merged.
            if (shifted.ifInvalidArguments()) {
                completion.invalidArguments();
            }
            if (shifted.noParameterExpected()) {
                completion.none();
            }
        }
    }
}

int list_commands(Output output)
{
    for (const auto& cmd : *Commands::get()) {
        output << cmd.first << endl;
    }
    return 0;
}

int completeCommand(Input input, Output output)
{
    int position = 0;
    ArgParse ap = ArgParse().mandatory(position);
    if (ap.parsingFails(input, output)) {
        return ap.exitCode();
    }
    bool shellQuoting = false;
    if (input.command() == "complete_shell") {
        shellQuoting = true;
    }
    Completion completion(ArgList(input.begin(), input.end()), position, "", shellQuoting, output);
    Commands::complete(completion);
    if (completion.ifInvalidArguments()) {
        return HERBST_INVALID_ARGUMENT;
    }
    if (completion.noParameterExpected()) {
        return HERBST_NO_PARAMETER_EXPECTED;
    }
    return 0;
}
