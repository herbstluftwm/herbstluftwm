/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_SETTINGS_H_
#define __HERBSTLUFT_SETTINGS_H_

#include "glib-backports.h"
#include "x11-types.h"
#include "utils.h"

enum {
    HS_String = 0,
    HS_Int,
    HS_Compatiblity,
};

typedef struct {
    const char*   name;
    union {
        int         i;
        const char* s_init;
        GString*    str;
        struct {
            const char* read;  // attribute address for reading
            const char* write; // attribute address for writing
        } compat;
    }   value;
    int old_value_i;
    int type;
    void (*on_change)(); // what to call on change
} SettingsPair;

extern int g_initial_monitors_locked;

void settings_init();
void settings_destroy();

SettingsPair* settings_find(const char* name);
SettingsPair* settings_get_by_index(int i);
char* settings_find_string(const char* name);

int settings_set(SettingsPair* pair, const char* value);
int settings_set_command(int argc, const char** argv, Output output);
int settings_toggle(int argc, char** argv, Output output);
int settings_cycle_value(int argc, char** argv, Output output);
int settings_count();
int settings_get(int argc, char** argv, Output output);

#endif

