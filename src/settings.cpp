/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "root.h"
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
    : window_border_width("window_border_width",
                          getIntAttr(root, "theme.tiling.active.border_width"),
                          setIntAttr(root, "theme.border_width"))
    , window_border_inner_width("window_border_inner_width",
                                getIntAttr(root, "theme.tiling.active.inner_width"),
                                setIntAttr(root, "theme.inner_width"))
    , window_border_inner_color("window_border_inner_color",
                                getColorAttr(root, "theme.tiling.active.inner_color"),
                                setColorAttr(root, "theme.inner_color"))
    , window_border_active_color("window_border_active_color",
                                 getColorAttr(root, "theme.tiling.active.color"),
                                 setColorAttr(root, "theme.active.color"))
    , window_border_normal_color("window_border_normal_color",
                                 getColorAttr(root, "theme.tiling.normal.color"),
                                 setColorAttr(root, "theme.normal.color"))
    , window_border_urgent_color("window_border_urgent_color",
                                 getColorAttr(root, "theme.tiling.urgent.color"),
                                 setColorAttr(root, "theme.urgent.color"))
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
    for (auto i : {&frame_gap, &frame_padding, &window_gap}) {
        i->setWriteable();
        i->changed().connect([] { all_monitors_apply_layout(); });
    }
    for (auto i : {&frame_border_normal_color,
         &frame_border_inner_color,
         &frame_bg_normal_color,
         &frame_bg_active_color}) {
        i->setWriteable();
        i->changed().connect(&reset_client_colors);
    }
    frame_bg_transparent.changed().connect(&reset_client_colors);
    for (auto i : {&frame_transparent_width,
         &frame_border_width,
         &frame_border_inner_width,
         &frame_active_opacity,
         &frame_normal_opacity}) {
        i->setWriteable();
        i->changed().connect(&reset_client_colors);
    }
    frame_bg_transparent.setWriteable();
    for (auto i : {&always_show_frame,
         &gapless_grid,
         &smart_frame_surroundings,
         &smart_window_surroundings}) {
        i->setWriteable();
        i->changed().connect(&all_monitors_apply_layout);
    }
    raise_on_focus_temporarily.setWriteable();
    raise_on_focus_temporarily.changed().connect(&tag_update_each_focus_layer);
    wmname.setWriteable();
    wmname.changed().connect(&ewmh_update_wmname);

    default_frame_layout.setValidator([] (int layout) {
        if (layout >= LAYOUT_COUNT) {
            return "layout number must be at most " + to_string(LAYOUT_COUNT - 1);
        }
        return std::string();
    });
    tree_style.setValidator([] (std::string tree_style) {
        if (utf8_string_length(tree_style) < 8) {
            return std::string("tree_style needs 8 characters");
        }
        return std::string();
    });

    // TODO: the lock level is not a setting! should move somewhere else
    monitors_locked = root->globals.initial_monitors_locked;
    monitors_locked.changed().connect([root](bool) {
        root->monitors()->lock_number_changed();
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


