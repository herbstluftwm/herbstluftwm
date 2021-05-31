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
        out.error() << "error: Command \"" << args.command() << "\" not found" << endl;
        return HERBST_COMMAND_NOT_FOUND;
    }
    // new channels object to have the command name updated
    OutputChannels channels(args.command(), out.output(), out.error());
    return cmd->second(args, channels);
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
            completion.mergeResultsFrom(shifted);
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
