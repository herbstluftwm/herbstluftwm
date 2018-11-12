#ifndef __HERBSTLUFT_COMMAND_H_
#define __HERBSTLUFT_COMMAND_H_

#include "glib-backports.h"
#include "x11-types.h"
#include "types.h"
// standard
#include <string>
#include <functional>
#include <unordered_map>

using namespace std;

// returns a command binding that internalizes object to given a command that
// calls the member function of the given object
#define BIND_OBJECT(OBJECT, MEMBER) \
    (CommandBinding([OBJECT](ArgList in, Output out) { \
        return OBJECT->MEMBER(in, out); \
    }))

#define BIND_PARAMETER(PARAM, FUNCTION) \
    (CommandBinding([PARAM](ArgList in, Output out) { \
        return FUNCTION(PARAM, in, out); \
    }))

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
    CommandBinding(function<int(ArgList, Output)> cmd)
        : command(cmd) {}
    // A command that taks an argument list and produces output
    CommandBinding(int cmd(ArgList, Output))
        : command(cmd) {}
    // A command that doesn't produce ouput
    CommandBinding(int cmd(ArgList))
        // Ignore the output parameter
        : command([cmd](ArgList args, Output) { return cmd(args); })
    {}

    // FIXME: Remove after C++ transition
    // The following constructors are only there to ease the transition from
    // C functions to C++
    CommandBinding(int func(int argc, char** argv, Output output))
        : command(commandFromCFunc(func)) {}
    CommandBinding(int func(int argc, const char** argv, Output output));
    CommandBinding(int func(int argc, char** argv));
    CommandBinding(int func(int argc, const char** argv));
    // Some functions take no arguments. I don't know if that's an accident,
    // because int foo() in C++ doesn't mean "unspecified many". Anyway,
    // CommandBinding(quit) doesn't typecheck without this constructor.
    CommandBinding(int func());

    /** Call the stored command */
    int operator()(ArgList args, Output out) const { return command(args, out); }

private:
    // FIXME: Remove after C++ transition
    function<int(ArgList,Output)> commandFromCFunc(
        function <int(int argc, char**argv, Output output)> func
    );

    function<int(ArgList, Output)> command;
};

class CommandTable {
    using Container = unordered_map<string, CommandBinding>;

public:
    CommandTable(initializer_list<Container::value_type> values)
        : map(values) {}

    int callCommand(ArgList args, Output out) const;

    Container::const_iterator begin() const { return map.cbegin(); }
    Container::const_iterator end() const { return map.cend(); }
private:
    Container map;
};

namespace Commands {
    void initialize(unique_ptr<const CommandTable> commands);
    /* Call the command args[0] */
    int call(ArgList args, Output out);
    shared_ptr<const CommandTable> get();
}

// Mark the following two functions as obsolete to make it easier to detect and
// fix call-sites gradually.

int call_command(int argc, char** argv, Output output)
   /* __attribute__((deprecated("Old C interface, use CommandTable"))) */;

int call_command_no_output(int argc, char** argv)
   /* __attribute__((deprecated("Old C interface, use CommandTable"))) */;

// commands
int list_commands(int argc, char** argv, Output output);
int complete_command(int argc, char** argv, Output output);

void try_complete(const char* needle, const char* to_check, Output output);
void try_complete_partial(const char* needle, const char* to_check, Output output);
void try_complete_prefix_partial(const char* needle, const char* to_check,
                                 const char* prefix, Output output);
void try_complete_prefix_partial(const std::string& needle, const std::string& to_check,
                                 const std::string& prefix, Output output);
void try_complete_prefix(const char* needle, const char* to_check,
                         const char* prefix, Output output);

void complete_settings(char* str, Output output);
void complete_against_list(char* needle, char** list, Output output);
void complete_against_tags(int argc, char** argv, int pos, Output output);
void complete_against_monitors(int argc, char** argv, int pos, Output output);
void complete_against_objects(int argc, char** argv, int pos, Output output);
void complete_against_attributes(int argc, char** argv, int pos, Output output);
void complete_against_user_attributes(int argc, char** argv, int pos, Output output);
void complete_against_attribute_values(int argc, char** argv, int pos, Output output);
void complete_against_comparators(int argc, char** argv, int pos, Output output);
void complete_against_winids(int argc, char** argv, int pos, Output output);
void complete_merge_tag(int argc, char** argv, int pos, Output output);
void complete_negate(int argc, char** argv, int pos, Output output);
void complete_against_settings(int argc, char** argv, int pos, Output output);
void complete_against_keybinds(int argc, char** argv, int pos, Output output);
int complete_against_commands(int argc, char** argv, int position,
                              Output output);
void complete_against_commands_1(int argc, char** argv, int position,
                                 Output output);
void complete_against_commands_3(int argc, char** argv, int position,
                                 Output output);
void complete_against_arg_1(int argc, char** argv, int position, Output output);
void complete_against_arg_2(int argc, char** argv, int position, Output output);
void complete_against_keybind_command(int argc, char** argv, int position,
                                      Output output);
void complete_against_mouse_combinations(int argc, char** argv, int position,
                                      Output output);

void complete_against_env(int argc, char** argv, int position, Output output);
void complete_chain(int argc, char** argv, int position, Output output);

int command_chain(char* separator, bool (*condition)(int laststatus),
                  int argc, char** argv, Output output);

void complete_sprintf(int argc, char** argv, int position, Output output);

void complete_against_user_attr_prefix(int argc, char** argv, int position,
                                       Output output);

int command_chain_command(int argc, char** argv, Output output);

int negate_command(int argc, char** argv, Output output);
#endif

