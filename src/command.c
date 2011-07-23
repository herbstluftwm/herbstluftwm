
#include "ipc-protocol.h"
#include "command.h"

#include <glib.h>
#include <string.h>


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

