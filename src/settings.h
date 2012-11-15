/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_SETTINGS_H_
#define __HERBSTLUFT_SETTINGS_H_

#include <glib.h>

enum {
    HS_String = 0,
    HS_Int
};

typedef struct {
    char*   name;
    union {
        int     i;
        char*   s;
    }   value;
    int old_value_i;
    int type;
    void (*on_change)(); // what to call on change
} SettingsPair;

extern SettingsPair g_settings[];
int g_initial_monitors_locked;

void settings_init();
void settings_destroy();

SettingsPair* settings_find(char* name);

int settings_set(SettingsPair* pair, char* value);
int settings_set_command(int argc, char** argv, GString* output);
int settings_toggle(int argc, char** argv, GString* output);
int settings_cycle_value(int argc, char** argv, GString* output);
int settings_count();
int settings_get(int argc, char** argv, GString* output);

#endif

