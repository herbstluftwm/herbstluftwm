/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "globals.h"
#include "settings.h"
#include "clientlist.h"
#include "layout.h"
#include "ipc-protocol.h"
#include "utils.h"
#include "ewmh.h"

#include <glib.h>
#include <string.h>
#include <stdio.h>

#define SET_INT(NAME, DEFAULT, CALLBACK) \
    { .name = (NAME), \
      .value = { .i = (DEFAULT) }, \
      .type = HS_Int, \
      .on_change = (CALLBACK), \
    }

#define SET_STRING(NAME, DEFAULT, CALLBACK) \
    { .name = (NAME), \
      .value = { .s = (DEFAULT) }, \
      .type = HS_String, \
      .on_change = (CALLBACK), \
    }

// often used callbacks:
#define RELAYOUT all_monitors_apply_layout
#define FR_COLORS reset_frame_colors
#define CL_COLORS reset_client_colors
#define LOCK_CHANGED monitors_lock_changed
#define FOCUS_LAYER tag_update_each_focus_layer
#define WMNAME ewmh_update_wmname

// default settings:
SettingsPair g_settings[] = {
    SET_INT(    "frame_gap",                       5,           RELAYOUT    ),
    SET_INT(    "frame_padding",                   0,           RELAYOUT    ),
    SET_INT(    "window_gap",                      0,           RELAYOUT    ),
    SET_INT(    "snap_distance",                   10,          NULL        ),
    SET_INT(    "snap_gap",                        5,           NULL        ),
    SET_INT(    "mouse_recenter_gap",              0,           NULL        ),
    SET_STRING( "frame_border_active_color",       "red",       FR_COLORS   ),
    SET_STRING( "frame_border_normal_color",       "blue",      FR_COLORS   ),
    SET_STRING( "frame_border_inner_color",        "black",     FR_COLORS   ),
    SET_STRING( "frame_bg_normal_color",           "black",     FR_COLORS   ),
    SET_STRING( "frame_bg_active_color",           "black",     FR_COLORS   ),
    SET_INT(    "frame_bg_transparent",            0,           FR_COLORS   ),
    SET_INT(    "frame_border_width",              2,           FR_COLORS   ),
    SET_INT(    "frame_border_inner_width",        0,           FR_COLORS   ),
    SET_INT(    "frame_active_opacity",            100,         FR_COLORS   ),
    SET_INT(    "frame_normal_opacity",            100,         FR_COLORS   ),
    SET_INT(    "window_border_width",             2,           CL_COLORS   ),
    SET_INT(    "window_border_inner_width",       0,           CL_COLORS   ),
    SET_STRING( "window_border_active_color",      "red",       CL_COLORS   ),
    SET_STRING( "window_border_normal_color",      "blue",      CL_COLORS   ),
    SET_STRING( "window_border_urgent_color",      "orange",    CL_COLORS   ),
    SET_STRING( "window_border_inner_color",       "black",     CL_COLORS   ),
    SET_INT(    "always_show_frame",               0,           RELAYOUT    ),
    SET_INT(    "default_direction_external_only", 0,           NULL        ),
    SET_INT(    "default_frame_layout",            0,           FR_COLORS   ),
    SET_INT(    "focus_follows_mouse",             0,           NULL        ),
    SET_INT(    "focus_stealing_prevention",       1,           NULL        ),
    SET_INT(    "swap_monitors_to_get_tag",        1,           NULL        ),
    SET_INT(    "raise_on_focus",                  0,           NULL        ),
    SET_INT(    "raise_on_focus_temporarily",      0,           FOCUS_LAYER ),
    SET_INT(    "raise_on_click",                  1,           NULL        ),
    SET_INT(    "gapless_grid",                    1,           RELAYOUT    ),
    SET_INT(    "smart_frame_surroundings",        0,           RELAYOUT    ),
    SET_INT(    "smart_window_surroundings",       0,           RELAYOUT    ),
    SET_INT(    "monitors_locked",                 0,           LOCK_CHANGED),
    SET_INT(    "auto_detect_monitors",            0,           NULL        ),
    SET_STRING( "tree_style",                      "*| +`--.",  FR_COLORS   ),
    SET_STRING( "wmname",                  WINDOW_MANAGER_NAME, WMNAME      ),
};

int             g_initial_monitors_locked = 0;

int settings_count() {
    return LENGTH(g_settings);
}

void settings_init() {
    // recreate all strings -> move them to heap
    int i;
    for (i = 0; i < LENGTH(g_settings); i++) {
        if (g_settings[i].type == HS_String) {
            g_settings[i].value.s = g_strdup(g_settings[i].value.s);
        }
        if (g_settings[i].type == HS_Int) {
            g_settings[i].old_value_i = 1;
        }
    }
    settings_find("monitors_locked")->value.i = g_initial_monitors_locked;
}

void settings_destroy() {
    // free all strings
    int i;
    for (i = 0; i < LENGTH(g_settings); i++) {
        if (g_settings[i].type == HS_String) {
            g_free(g_settings[i].value.s);
        }
    }
}

SettingsPair* settings_find(char* name) {
    return STATIC_TABLE_FIND_STR(SettingsPair, g_settings, name, name);
}

int settings_set_command(int argc, char** argv, GString* output) {
    if (argc < 3) {
        return HERBST_NEED_MORE_ARGS;
    }
    SettingsPair* pair = settings_find(argv[1]);
    if (!pair) {
        if (output != NULL) {
            g_string_append_printf(output,
                "%s: Setting \"%s\" not found\n", argv[0], argv[1]);
        }
        return HERBST_SETTING_NOT_FOUND;
    }
    int ret = settings_set(pair, argv[2]);
    if (ret == HERBST_INVALID_ARGUMENT) {
        g_string_append_printf(output,
            "%s: Invalid value for setting \"%s\"\n", argv[0], argv[1]);
    }
    return ret;
}

int settings_set(SettingsPair* pair, char* value) {
    if (pair->type == HS_Int) {
        int new_value;
        // parse value to int, if possible
        if (1 == sscanf(value, "%d", &new_value)) {
            if (new_value == pair->value.i) {
                // nothing would be changed
                return 0;
            }
            pair->value.i = new_value;
        } else {
            return HERBST_INVALID_ARGUMENT;
        }
    } else { // pair->type == HS_String
        if (!strcmp(pair->value.s, value)) {
            // nothing would be changed
            return 0;
        }
        g_free(pair->value.s);
        pair->value.s = g_strdup(value);
    }
    // on successful change, call callback
    if (pair->on_change) {
        pair->on_change();
    }
    return 0;
}

int settings_get(int argc, char** argv, GString* output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    SettingsPair* pair = settings_find(argv[1]);
    if (!pair) {
        g_string_append_printf(output,
            "%s: Setting \"%s\" not found\n", argv[0], argv[1]);
        return HERBST_SETTING_NOT_FOUND;
    }
    if (pair->type == HS_Int) {
        g_string_append_printf(output, "%d", pair->value.i);
    } else { // pair->type == HS_String
        g_string_append(output, pair->value.s);
    }
    return 0;
}

// toggle integer-like values
int settings_toggle(int argc, char** argv, GString* output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    SettingsPair* pair = settings_find(argv[1]);
    if (!pair) {
        g_string_append_printf(output,
            "%s: Setting \"%s\" not found\n", argv[0], argv[1]);
        return HERBST_SETTING_NOT_FOUND;
    }
    if (pair->type == HS_Int) {
        if (pair->value.i) {
            /* store old value */
            pair->old_value_i = pair->value.i;
            pair->value.i = 0;
        } else {
            /* recover old value */
            pair->value.i = pair->old_value_i;
        }
    } else {
        // only toggle numbers
        g_string_append_printf(output,
            "%s: Only numbers can be toggled\n", argv[0]);
        return HERBST_INVALID_ARGUMENT;
    }
    // on successful change, call callback
    if (pair->on_change) {
        pair->on_change();
    }
    return 0;
}
bool memberequals_settingspair(void* pmember, void* needle) {
    char* str = *(char**)pmember;
    SettingsPair* pair = needle;
    if (pair->type == HS_Int) {
        return pair->value.i == atoi(str);
    } else {
        return !strcmp(pair->value.s, str);
    }
}

int settings_cycle_value(int argc, char** argv, GString* output) {
    if (argc < 3) {
        return HERBST_NEED_MORE_ARGS;
    }
    char* cmd_name = argv[0];
    char* setting_name = argv[1]; // save this before shifting
    SettingsPair* pair = settings_find(argv[1]);
    if (!pair) {
        g_string_append_printf(output,
            "%s: Setting \"%s\" not found\n", argv[0], argv[1]);
        return HERBST_SETTING_NOT_FOUND;
    }
    (void)SHIFT(argc, argv);
    (void)SHIFT(argc, argv);
    char** pcurrent = table_find(argv, sizeof(*argv), argc, 0,
                                 memberequals_settingspair, pair);
    int i = pcurrent ? ((INDEX_OF(argv, pcurrent) + 1) % argc) : 0;
    int ret = settings_set(pair, argv[i]);
    if (ret == HERBST_INVALID_ARGUMENT) {
        g_string_append_printf(output,
            "%s: Invalid value for setting \"%s\"\n", cmd_name, setting_name);
    }
    return ret;
}

