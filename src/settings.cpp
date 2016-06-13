/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "globals.h"
#include "settings.h"
#include "client.h"
#include "layout.h"
#include "ipc-protocol.h"
#include "ewmh.h"

#include "glib-backports.h"
#include <string.h>
#include <stdio.h>
#include <sstream>

SettingsPair SET_INT(const char* name, int defaultval, void (*cb)())
{
    SettingsPair sp;
    sp.name = name;
    sp.value.i = defaultval;
    sp.type = HS_Int;
    sp.on_change = (cb);
    return sp;
}

SettingsPair SET_STRING(const char* name, const char* defaultval, void (*cb)())
{
    SettingsPair sp;
    sp.name = name;
    sp.value.s_init = defaultval;
    sp.type = HS_String;
    sp.on_change = (cb);
    return sp;
}

SettingsPair SET_COMPAT(const char* name, const char* read, const char* write)
{
    SettingsPair sp;
    sp.name = name;
    sp.value.compat.read = read;
    sp.value.compat.write = write;
    sp.type = HS_Compatiblity;
    sp.on_change = NULL;
    return sp;
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
    SET_INT(    "frame_transparent_width",         0,           FR_COLORS   ),
    SET_INT(    "frame_border_width",              2,           FR_COLORS   ),
    SET_INT(    "frame_border_inner_width",        0,           FR_COLORS   ),
    SET_INT(    "frame_active_opacity",            100,         FR_COLORS   ),
    SET_INT(    "frame_normal_opacity",            100,         FR_COLORS   ),
    SET_INT(    "focus_crosses_monitor_boundaries", 1,          NULL        ),
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
    SET_INT(    "pseudotile_center_threshold",    10,           RELAYOUT    ),
    SET_INT(    "update_dragged_clients",          0,           NULL        ),
    SET_STRING( "tree_style",                      "*| +`--.",  FR_COLORS   ),
    SET_STRING( "wmname",                  WINDOW_MANAGER_NAME, WMNAME      ),
    // settings for compatibility:
    SET_COMPAT( "window_border_width",
                "theme.tiling.active.border_width", "theme.border_width"),
    SET_COMPAT( "window_border_inner_width",
                "theme.tiling.active.inner_width", "theme.inner_width"),
    SET_COMPAT( "window_border_inner_color",
                "theme.tiling.active.inner_color", "theme.inner_color"),
    SET_COMPAT( "window_border_active_color",
                "theme.tiling.active.color", "theme.active.color"),
    SET_COMPAT( "window_border_normal_color",
                "theme.tiling.normal.color", "theme.normal.color"),
    SET_COMPAT( "window_border_urgent_color",
                "theme.tiling.urgent.color", "theme.urgent.color"),
};

// globals:
int g_initial_monitors_locked = 0;

int settings_count() {
    return LENGTH(g_settings);
}

void settings_init() {
    // recreate all strings -> move them to heap
    for (int i = 0; i < LENGTH(g_settings); i++) {
        if (g_settings[i].type == HS_String) {
            g_settings[i].value.str = g_string_new(g_settings[i].value.s_init);
        }
        if (g_settings[i].type == HS_Int) {
            g_settings[i].old_value_i = 1;
        }
    }
    settings_find("monitors_locked")->value.i = g_initial_monitors_locked;

    // create a settings object
    // ensure everything is nulled that is not explicitely initialized
    // TODO re-initialize settings here
}

void settings_destroy() {
    // free all strings
    int i;
    for (i = 0; i < LENGTH(g_settings); i++) {
        if (g_settings[i].type == HS_String) {
            g_string_free(g_settings[i].value.str, true);
        }
    }
}

SettingsPair* settings_find(const char* name) {
    return STATIC_TABLE_FIND_STR(SettingsPair, g_settings, name, name);
}

SettingsPair* settings_get_by_index(int i) {
    if (i < 0 || i >= LENGTH(g_settings)) return NULL;
    return g_settings + i;
}

char* settings_find_string(const char* name) {
    SettingsPair* sp = settings_find(name);
    if (!sp) return NULL;
    HSWeakAssert(sp->type == HS_String);
    if (sp->type != HS_String) return NULL;
    return sp->value.str->str;
}

int settings_set_command(int argc, const char** argv, Output output) {
    if (argc < 3) {
        return HERBST_NEED_MORE_ARGS;
    }
    SettingsPair* pair = settings_find(argv[1]);
    if (!pair) {
        output << argv[0] << ": Setting \"" << argv[1] << "\" not found\n";
        return HERBST_SETTING_NOT_FOUND;
    }
    int ret = settings_set(pair, argv[2]);
    if (ret == HERBST_INVALID_ARGUMENT) {
        output << argv[0] << ": Invalid value for setting \"" << argv[1] << "\"\n";
    }
    return ret;
}

int settings_set(SettingsPair* pair, const char* value) {
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
    } else if (pair->type == HS_String) {
        if (!strcmp(pair->value.str->str, value)) {
            // nothing would be changed
            return 0;
        }
        g_string_assign(pair->value.str, value);
    } else if (pair->type == HS_Compatiblity) {
        // TODO
        return 1;
    }
    // on successful change, call callback
    if (pair->on_change) {
        pair->on_change();
    }
    return 0;
}

int settings_get(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    SettingsPair* pair = settings_find(argv[1]);
    if (!pair) {
        output << argv[0] <<
            ": Setting \"" << argv[1] << "\" not found\n";
        return HERBST_SETTING_NOT_FOUND;
    }
    if (pair->type == HS_Int) {
        output << pair->value.i;
    } else if (pair->type == HS_String) {
        output << pair->value.str->str;
    } else if (pair->type == HS_Compatiblity) {
        return 1;
    }
    return 0;
}

// toggle integer-like values
int settings_toggle(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    SettingsPair* pair = settings_find(argv[1]);
    if (!pair) {
        output << argv[0] <<
            ": Setting \"" << argv[1] << "\" not found\n";
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
        output << argv[0] << ": Only numbers can be toggled\n";
        return HERBST_INVALID_ARGUMENT;
    }
    // on successful change, call callback
    if (pair->on_change) {
        pair->on_change();
    }
    return 0;
}
static bool memberequals_settingspair(void* pmember, const void* needle) {
    char* str = *(char**)pmember;
    const SettingsPair* pair = (const SettingsPair*) needle;
    if (pair->type == HS_Int) {
        return pair->value.i == atoi(str);
    } else {
        return !strcmp(pair->value.str->str, str);
    }
}

int settings_cycle_value(int argc, char** argv, Output output) {
    if (argc < 3) {
        return HERBST_NEED_MORE_ARGS;
    }
    char* cmd_name = argv[0];
    char* setting_name = argv[1]; // save this before shifting
    SettingsPair* pair = settings_find(argv[1]);
    if (!pair) {
        output << argv[0] << ": Setting \"" << argv[1] << "\" not found\n";
        return HERBST_SETTING_NOT_FOUND;
    }
    (void)SHIFT(argc, argv);
    (void)SHIFT(argc, argv);
    char** pcurrent = (char**)table_find(argv, sizeof(*argv), argc, 0,
                                 memberequals_settingspair, pair);
    int i = pcurrent ? ((INDEX_OF(argv, pcurrent) + 1) % argc) : 0;
    int ret = settings_set(pair, argv[i]);
    if (ret == HERBST_INVALID_ARGUMENT) {
        output << cmd_name <<
            ": Invalid value for setting \"" << setting_name << "\"\n";
    }
    return ret;
}

