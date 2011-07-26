
#include "ipc-protocol.h"
#include "command.h"
#include "utils.h"
#include "settings.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>


int call_command(int argc, char** argv, GString** output) {
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

int call_command_no_ouput(int argc, char** argv) {
    GString* output = g_string_new("");
    int status = call_command(argc, argv, &output);
    g_string_free(output, true);
    return status;
}


int list_commands(int argc, char** argv, GString** output)
{
    int i = 0;
    while (g_commands[i].cmd.standard != NULL) {
        *output = g_string_append(*output, g_commands[i].name);
        *output = g_string_append(*output, "\n");
        i++;
    }
    return 0;
}

int complete_command(int argc, char** argv, GString** output) {
    // usage: complete POSITION command to complete ...
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }
    // index must be between first and als arg of "commmand to complete ..."
    int position = CLAMP(atoi(argv[1]), 0, argc-2);
    // complete command
    if (position == 0) {
        char* str = (argc >= 3) ? argv[2] : "";
        size_t len = strlen(str);
        int i = 0;
        while (g_commands[i].cmd.standard != NULL) {
            // only check the first len bytes
            if (!strncmp(str, g_commands[i].name, len)) {
                *output = g_string_append(*output, g_commands[i].name);
                *output = g_string_append(*output, "\n");
            }
            i++;
        }
    }
    bool is_toggle_command = !strcmp(argv[2], "toggle");
    if (position == 1 &&
        (!strcmp(argv[2], "set") || !strcmp(argv[2], "get") || is_toggle_command)) {
        // complete with setting name
        char* str = (argc >= 4) ? argv[3] : "";
        size_t len = strlen(str);
        int i;
        for (i = 0; i < settings_count(); i++) {
            if (is_toggle_command && g_settings[i].type != HS_Int) {
                continue;
            }
            printf("check: %s\n", g_settings[i].name);
            // only check the first len bytes
            if (!strncmp(str, g_settings[i].name, len)) {
                *output = g_string_append(*output, g_settings[i].name);
                *output = g_string_append(*output, "\n");
            }
        }
    }
    return 0;
}





