/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_COMMAND_H_
#define __HERBSTLUFT_COMMAND_H_

#include "glib-backports.h"
#include <stdbool.h>
#include "x11-types.h"

typedef int (*HerbstCmd)(int argc,      // number of arguments
                         const char** argv,   // array of args
                         Output output  // result-data/stdout
                        );
typedef int (*HerbstCmdNoOutput)(int argc,  // number of arguments
                         const char** argv        // array of args
                        );

#define CMD_BIND(NAME, FUNC) \
    { CommandBindingCB(FUNC), (NAME), 1 }
#define CMD_BIND_NO_OUTPUT(NAME, FUNC) \
    { CommandBindingCB(FUNC), (NAME), 0 }

union CommandBindingCB {
    HerbstCmd standard;
    HerbstCmdNoOutput no_output;
    CommandBindingCB() : standard(NULL) { };
    CommandBindingCB(HerbstCmd x) : standard(x) { };
    CommandBindingCB(int (*x)(int,char**,Output)) : standard((HerbstCmd)x) { };
    CommandBindingCB(HerbstCmdNoOutput x) : no_output(x) { };
    CommandBindingCB(int (*x)(int,char**)) : no_output((HerbstCmdNoOutput)x) { };
    CommandBindingCB(int (*x)()) : no_output((HerbstCmdNoOutput)x) { };
};

typedef struct CommandBinding {
    CommandBindingCB cmd;
    const char* name;
    bool        has_output;
} CommandBinding;

extern CommandBinding g_commands[];

int call_command(int argc, char** argv, Output output);
int call_command_no_output(int argc, char** argv);
int call_command_substitute(char* needle, char* replacement,
                            int argc, char** argv, Output output);

// commands
int list_commands(int argc, char** argv, Output output);
int complete_command(int argc, char** argv, Output output);

void try_complete(const char* needle, const char* to_check, Output output);
void try_complete_partial(const char* needle, const char* to_check, Output output);
void try_complete_prefix_partial(const char* needle, const char* to_check,
                                 const char* prefix, Output output);
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

