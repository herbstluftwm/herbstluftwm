/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_COMMAND_H_
#define __HERBSTLUFT_COMMAND_H_

#include <glib.h>
#include <stdbool.h>

typedef int (*HerbstCmd)(int argc,      // number of arguments
                         char** argv,   // array of args
                         GString* output  // result-data/stdout
                        );
typedef int (*HerbstCmdNoOutput)(int argc,  // number of arguments
                         char** argv        // array of args
                        );

#define CMD_BIND(NAME, FUNC) \
    { .cmd = { .standard = (FUNC) }, .name = (NAME), .has_output = 1 }
#define CMD_BIND_NO_OUTPUT(NAME, FUNC) \
    { .cmd = { .no_output = (FUNC) }, .name = (NAME), .has_output = 0 }

typedef struct CommandBinding {
    union {
        HerbstCmd standard;
        HerbstCmdNoOutput no_output;
    } cmd;
    char*   name;
    bool    has_output;
} CommandBinding;

extern CommandBinding g_commands[];

int call_command(int argc, char** argv, GString* output);
int call_command_no_output(int argc, char** argv);

// commands
int list_commands(int argc, char** argv, GString* output);
int complete_command(int argc, char** argv, GString* output);

void try_complete(char* needle, char* to_check, GString* output);
void try_complete_partial(char* needle, char* to_check, GString* output);
void try_complete_prefix_partial(char* needle, char* to_check,
                                 char* prefix, GString* output);
void try_complete_prefix(char* needle, char* to_check,
                         char* prefix, GString* output);

void complete_settings(char* str, GString* output);
void complete_against_list(char* needle, char** list, GString* output);
void complete_against_tags(int argc, char** argv, int pos, GString* output);
void complete_against_monitors(int argc, char** argv, int pos, GString* output);
void complete_against_objects(int argc, char** argv, int pos, GString* output);
void complete_against_attributes(int argc, char** argv, int pos, GString* output);
void complete_against_attribute_values(int argc, char** argv, int pos, GString* output);
void complete_against_comparators(int argc, char** argv, int pos, GString* output);
void complete_against_winids(int argc, char** argv, int pos, GString* output);
void complete_merge_tag(int argc, char** argv, int pos, GString* output);
void complete_negate(int argc, char** argv, int pos, GString* output);
void complete_against_settings(int argc, char** argv, int pos, GString* output);
void complete_against_keybinds(int argc, char** argv, int pos, GString* output);
int complete_against_commands(int argc, char** argv, int position,
                              GString* output);
void complete_against_commands_3(int argc, char** argv, int position,
                                 GString* output);
void complete_against_arg_1(int argc, char** argv, int position, GString* output);
void complete_against_keybind_command(int argc, char** argv, int position,
                                      GString* output);
void complete_against_env(int argc, char** argv, int position, GString* output);
void complete_chain(int argc, char** argv, int position, GString* output);

int command_chain(char* separator, bool (*condition)(int laststatus),
                  int argc, char** argv, GString* output);


int command_chain_command(int argc, char** argv, GString* output);

int negate_command(int argc, char** argv, GString* output);
#endif

