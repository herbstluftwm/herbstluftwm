/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "root.h"
#include "globals.h"
#include "settings.h"
#include "client.h"
#include "layout.h"
#include "ipc-protocol.h"
#include "ewmh.h"
#include "monitormanager.h"

#include <string.h>
#include <stdio.h>
#include <sstream>


using namespace std;

Settings* g_settings = nullptr; // the global settings object

Settings::Settings(Root* root)
    : frame_gap(                "frame_gap",        AT_THIS(relayout), 5)
    , frame_padding(            "frame_padding",    AT_THIS(relayout),                   0)
    , window_gap(               "window_gap",       AT_THIS(relayout),                      0)
    , snap_distance(            "snap_distance",    ACCEPT_ALL,                   10)
    , snap_gap(                 "snap_gap",         ACCEPT_ALL,                        5)
    , mouse_recenter_gap(       "mouse_recenter_gap",           ACCEPT_ALL,              0)
    , frame_border_active_color("frame_border_active_color",    AT_THIS(fr_colors),       Color("red"))
    , frame_border_normal_color("frame_border_normal_color",    AT_THIS(fr_colors),       Color("blue"))
    , frame_border_inner_color( "frame_border_inner_color",     AT_THIS(fr_colors),        Color("black"))
    , frame_bg_normal_color(    "frame_bg_normal_color",        AT_THIS(fr_colors),           Color("black"))
    , frame_bg_active_color(    "frame_bg_active_color",        AT_THIS(fr_colors),           Color("black"))
    , frame_bg_transparent(     "frame_bg_transparent",         AT_THIS(fr_colors),            0)
    , frame_transparent_width(  "frame_transparent_width",      AT_THIS(fr_colors),         0)
    , frame_border_width(       "frame_border_width",           AT_THIS(fr_colors),              2)
    , frame_border_inner_width( "frame_border_inner_width",     AT_THIS(fr_colors),        0)
    , frame_active_opacity(     "frame_active_opacity",         AT_THIS(fr_colors),            100)
    , frame_normal_opacity(     "frame_normal_opacity",         AT_THIS(fr_colors),            100)
    , focus_crosses_monitor_boundaries("focus_crosses_monitor_boundaries", ACCEPT_ALL, 1)
    , always_show_frame(        "always_show_frame",            AT_THIS(relayout),               0)
    , default_direction_external_only("default_direction_external_only",           ACCEPT_ALL, 0)
    , default_frame_layout(     "default_frame_layout",         AT_THIS(fr_colors),            0)
    , focus_follows_mouse(      "focus_follows_mouse",          ACCEPT_ALL,             0)
    , focus_stealing_prevention("focus_stealing_prevention",    ACCEPT_ALL,       1)
    , swap_monitors_to_get_tag( "swap_monitors_to_get_tag",     ACCEPT_ALL,        1)
    , raise_on_focus(           "raise_on_focus",               ACCEPT_ALL,                  0)
    , raise_on_focus_temporarily("raise_on_focus_temporarily",  AT_THIS(focus_layer),      0)
    , raise_on_click(           "raise_on_click",               ACCEPT_ALL,                  1)
    , gapless_grid(             "gapless_grid",                 AT_THIS(relayout),                    1)
    , smart_frame_surroundings( "smart_frame_surroundings",     AT_THIS(relayout),        0)
    , smart_window_surroundings("smart_window_surroundings",    AT_THIS(relayout),       0)
    , monitors_locked(          "monitors_locked",              AT_THIS(lock_changed), root->globals.initial_monitors_locked)
    , auto_detect_monitors(     "auto_detect_monitors",         ACCEPT_ALL,            0)
    , pseudotile_center_threshold("pseudotile_center_threshold",AT_THIS(relayout),    10)
    , update_dragged_clients(   "update_dragged_clients",       ACCEPT_ALL,          0)
    , tree_style(               "tree_style",                   AT_THIS(onTreeStyleChange), "*| +`--.")
    , wmname(                   "wmname",                       AT_THIS(update_wmname),                  WINDOW_MANAGER_NAME)
    , window_border_width("window_border_width",
        setIntAttr(root, "theme.border_width"),
        getIntAttr(root, "theme.tiling.active.border_width"))
    , window_border_inner_width("window_border_inner_width",
        setIntAttr(root, "theme.inner_width"),
        getIntAttr(root, "theme.tiling.active.inner_width"))
    , window_border_inner_color("window_border_inner_color",
        setColorAttr(root, "theme.inner_color"),
        getColorAttr(root, "theme.tiling.active.inner_color"))
    , window_border_active_color("window_border_active_color",
        setColorAttr(root, "theme.active.color"),
        getColorAttr(root, "theme.tiling.active.color"))
    , window_border_normal_color("window_border_normal_color",
        setColorAttr(root, "theme.normal.color"),
        getColorAttr(root, "theme.tiling.normal.color"))
    , window_border_urgent_color("window_border_urgent_color",
        setColorAttr(root, "theme.urgent.color"),
        getColorAttr(root, "theme.tiling.urgent.color"))
    , root(root)
{
    wireAttributes({
        &frame_gap,
        &frame_padding,
        &window_gap,
        &snap_distance,
        &snap_gap,
        &mouse_recenter_gap,
        &frame_border_active_color,
        &frame_border_normal_color,
        &frame_border_inner_color,
        &frame_bg_normal_color,
        &frame_bg_active_color,
        &frame_bg_transparent,
        &frame_transparent_width,
        &frame_border_width,
        &frame_border_inner_width,
        &frame_active_opacity,
        &frame_normal_opacity,
        &focus_crosses_monitor_boundaries,
        &always_show_frame,
        &default_direction_external_only,
        &default_frame_layout,
        &focus_follows_mouse,
        &focus_stealing_prevention,
        &swap_monitors_to_get_tag,
        &raise_on_focus,
        &raise_on_focus_temporarily,
        &raise_on_click,
        &gapless_grid,
        &smart_frame_surroundings,
        &smart_window_surroundings,
        &monitors_locked,
        &auto_detect_monitors,
        &pseudotile_center_threshold,
        &update_dragged_clients,
        &tree_style,
        &wmname,

        &window_border_width,
        &window_border_inner_width,
        &window_border_inner_color,
        &window_border_active_color,
        &window_border_normal_color,
        &window_border_urgent_color,
    });

    g_settings = this;
}

std::function<int()> Settings::getIntAttr(Object* root, std::string name) {
    return [root, name]() {
        Attribute* a = root->deepAttribute(name);
        if (a) {
            return std::stoi(a->str());
        } else {
            HSDebug("Internal Error: No such attribute %s\n", name.c_str());
            return 0;
        }
    };
}

std::function<Color()> Settings::getColorAttr(Object* root, std::string name) {
    return [root, name]() {
        Attribute* a = root->deepAttribute(name);
        if (a) {
            return Color(a->str());
        } else {
            HSDebug("Internal Error: No such attribute %s\n", name.c_str());
            return Color("black");
        }
    };
}

std::function<string(int)> Settings::setIntAttr(Object* root, std::string name) {
    return [root, name](int val) {
        Attribute* a = root->deepAttribute(name);
        if (a) {
            return a->change(to_string(val));
        } else {
            string msg = "Internal Error: No such attribute ";
            msg += name;
            msg += "\"";
            return msg;
        }
    };
}
std::function<string(Color)> Settings::setColorAttr(Object* root, std::string name) {
    return [root, name](Color val) {
        Attribute* a = root->deepAttribute(name);
        if (a) {
            return a->change(val.str());
        } else {
            string msg = "Internal Error: No such attribute ";
            msg += name;
            msg += "\"";
            return msg;
        }
    };
}

string Settings::onTreeStyleChange() {
    if (utf8_string_length(tree_style()) < 8) {
        return "tree_style needs 8 characters";
    }
    return {};
}

string Settings::relayout() {
    all_monitors_apply_layout();
    return {};
}
string Settings::fr_colors() {
    if (default_frame_layout < 0 || default_frame_layout >= LAYOUT_COUNT) {
        return "layout number must be between 0 and " + to_string(LAYOUT_COUNT - 1);
    }
    reset_frame_colors();
    return {};
}
string Settings::cl_colors() {
    reset_client_colors();
    return {};
}
string Settings::lock_changed() {
    return root->monitors()->lock_number_changed();
}
string Settings::focus_layer() {
    tag_update_each_focus_layer();
    return {};
}
string Settings::update_wmname() {
    ewmh_update_wmname();
    return {};
}


int Settings::set_cmd(Input argv, Output output) {
    argv.shift();
    if (argv.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto set_name = argv.front();
    argv.shift();
    if (argv.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto value = argv.front();
    auto attr = attribute(set_name);
    if (!attr) {
        output << argv.command() <<
            ": Setting \"" << set_name << "\" not found\n";
        return HERBST_SETTING_NOT_FOUND;
    }
    auto msg = attr->change(value);
    if (msg != "") {
        output << argv.command()
               << ": Invalid value \"" << value
               << "\" for setting \"" << set_name << "\": "
               << msg << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    return 0;
}

int Settings::toggle_cmd(Input argv, Output output) {
    argv.shift();
    if (argv.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto set_name = argv.front();
    auto attr = attribute(set_name);
    if (!attr) {
        output << argv.command() <<
            ": Setting \"" << set_name << "\" not found\n";
        return HERBST_SETTING_NOT_FOUND;
    }
    if (attr->type() == Type::ATTRIBUTE_INT) {
        if (attr->str() == "0") {
            attr->change("1");
        } else {
            attr->change("0");
        }
    } else if (attr->type() == Type::ATTRIBUTE_BOOL) {
        attr->change("toggle");
    } else {
        output << argv.command()
            << ": Setting \"" << set_name
            << "\" is not of type integer or bool\n";
        return HERBST_INVALID_ARGUMENT;
    }
    return 0;
}

int Settings::cycle_value_cmd(Input argv, Output output) {
    argv.shift();
    if (argv.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto set_name = argv.front();
    argv.shift();
    if (argv.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto attr = attribute(set_name);
    if (!attr) {
        output << argv.command() <<
            ": Setting \"" << set_name << "\" not found\n";
        return HERBST_SETTING_NOT_FOUND;
    }
    auto msg = attr->cycleValue(argv.begin(), argv.end());
    if (msg != "") {
        output << argv.command()
               << ": Invalid value for setting \""
               << set_name << "\": "
               << msg << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    return 0;
}

int Settings::get_cmd(Input argv, Output output) {
    argv.shift();
    if (argv.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto attr = attribute(argv.front());
    if (!attr) {
        output << argv.command() <<
            ": Setting \"" << argv.front() << "\" not found\n";
        return HERBST_SETTING_NOT_FOUND;
    }
    output << attr->str();
    return 0;
}


