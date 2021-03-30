#ifndef __HERBSTLUFT_COMMAND_H_
#define __HERBSTLUFT_COMMAND_H_

#include <functional>
#include <string>
#include <unordered_map>

#include "commandio.h"

class Completion;

/** User facing commands.
 *
 * A command can have one of the two forms
 *    - (Input, Output) -> int
 *    - (Input) -> int
 * where the second one simply doesn't produce any output. Both return an error
 * code or 0 on success.
 *
 * This class is mainly used to provide convenience constructors for the
 * initialization of CommandTable.
 *
 * A command stored in here can be called with the () operator.
 */
class CommandBinding {
public:
    CommandBinding(std::function<int(Input, Output)> cmd)
        : command(cmd) {}
    // A command that takes an argument list and produces output
    CommandBinding(int cmd(Input, Output))
        : command(cmd) {}
    // A command that doesn't produce ouput
    CommandBinding(int cmd(Input))
        // Ignore the output parameter
        : command([cmd](Input args, Output) { return cmd(args); })
    {}
    // A command that doesn't have input
    CommandBinding(std::function<int(Output)> cmd);
    // A command that doesn't have input nor output
    CommandBinding(std::function<int()> cmd);
    // A regular command and its completion
    CommandBinding(std::function<int(Input, Output)> cmd,
                   std::function<void(Completion&)> completion)
        : command(cmd)
        , completion_(completion)
    {}

    /** Binding to a command in a given object, together with
     * a completion function in the same object
     */
    template <typename ClassName>
    CommandBinding(ClassName* object,
                   int(ClassName::*member_cmd)(Input,Output),
                   void (ClassName::*completer)(Completion&))
        : command(std::bind(member_cmd, object,
                            std::placeholders::_1, std::placeholders::_2))
        , completion_(std::bind(completer, object,
                                std::placeholders::_1))
    {
    }
    /** Binding to a command in a given object, but with no input
     * parameters and thus without completion.
     */
    template <typename ClassName>
    CommandBinding(ClassName* object,
                   int(ClassName::*member_cmd)(Output))
        : CommandBinding(std::bind(member_cmd, object,
                            std::placeholders::_1))
    {
    }

    /** Same as before, but with a const member function
     */
    template <typename ClassName>
    CommandBinding(ClassName* object,
                   int(ClassName::*member_cmd)(Output) const)
        : CommandBinding(std::bind(member_cmd, object,
                            std::placeholders::_1))
    {
    }

    /** Binding to a combined command invokation and completion
     *  function.
     */
    CommandBinding(std::function<void(CallOrComplete)> callOrCompl)
    {
        command = [callOrCompl](Input input, Output output) -> int {
            std::pair<Input, Output> io = {input, output};
            int exitCode = 0;
            CallOrComplete invoc;
            invoc.command_ = input.command();
            invoc.inputOutput_ = &io;
            invoc.exitCode_ = &exitCode;
            callOrCompl(invoc);
            return exitCode;
        };
        completion_ = [callOrCompl](Completion& complete) {
            CallOrComplete invoc;
            invoc.complete_ = &complete;
            callOrCompl(invoc);
        };
    }

    template <typename ClassName>
    CommandBinding(ClassName* object,
                   void(ClassName::*member)(CallOrComplete))
        : CommandBinding(std::bind(member, object,
                            std::placeholders::_1))
    {
    }

    // FIXME: Remove after C++ transition
    // The following constructors are only there to ease the transition from
    // C functions to C++
    CommandBinding(int func(int argc, char** argv, Output output))
        : command(commandFromCFunc(func)) {}
    CommandBinding(int func(int argc, char** argv));

    bool hasCompletion() const { return (bool)completion_; }
    void complete(Completion& completion) const;

    /** Call the stored command */
    int operator()(Input args, Output out) const { return command(args, out); }

private:
    // FIXME: Remove after C++ transition
    std::function<int(Input,Output)> commandFromCFunc(
        std::function <int(int argc, char**argv, Output output)> func
    );

    std::function<int(Input, Output)> command;
    std::function<void(Completion&)>  completion_;
};

class CommandTable {
    using Container = std::unordered_map<std::string, CommandBinding>;

public:
    CommandTable(std::initializer_list<Container::value_type> values)
        : map(values) {}

    int callCommand(Input args, Output out) const;

    Container::const_iterator begin() const { return map.cbegin(); }
    Container::const_iterator end() const { return map.cend(); }
    Container::const_iterator find(const std::string& str) const { return map.find(str); }
private:
    Container map;
};

namespace Commands {
    void initialize(std::unique_ptr<const CommandTable> commands);
    /* Call the command args[0] */
    int call(Input args, Output out);
    bool commandExists(const std::string& commandName);
    void complete(Completion& completion);
    std::shared_ptr<const CommandTable> get();
}

// commands
int list_commands(Output output);
int complete_command(int argc, char** argv, Output output);

#endif

