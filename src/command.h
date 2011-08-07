/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_COMMAND_H_
#define __HERBSTLUFT_COMMAND_H_

#include "glib.h"
#include <stdbool.h>

typedef int (*HerbstCmd)(int argc,      // number of arguments
                         char** argv,   // array of args
                         GString** output  // result-data/stdout
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

int call_command(int argc, char** argv, GString** output);
int call_command_no_ouput(int argc, char** argv);

// commands
int list_commands(int argc, char** argv, GString** output);
int complete_command(int argc, char** argv, GString** output);

void complete_settings(char* str, GString** output);

#endif

