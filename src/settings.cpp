#include "settings.h"

#include <sstream>

#include "client.h"
#include "completion.h"
#include "ewmh.h"
#include "framedata.h"
#include "ipc-protocol.h"
#include "monitormanager.h"
#include "root.h"
#include "utils.h"

using std::endl;
using std::function;
using std::string;
using std::to_string;

Settings* g_settings = nullptr; // the global settings object

Settings::Settings()
    : window_border_width("window_border_width",
                          getIntAttr("theme.tiling.active.border_width"),
                          setIntAttr("theme.border_width"))
    , window_border_inner_width("window_border_inner_width",
                                getIntAttr("theme.tiling.active.inner_width"),
                                setIntAttr("theme.inner_width"))
    , window_border_inner_color("window_border_inner_color",
                                getColorAttr("theme.tiling.active.inner_color"),
                                setColorAttr("theme.inner_color"))
    , window_border_active_color("window_border_active_color",
                                 getColorAttr("theme.tiling.active.color"),
                                 setColorAttr("theme.active.color"))
    , window_border_normal_color("window_border_normal_color",
                                 getColorAttr("theme.tiling.normal.color"),
                                 setColorAttr("theme.normal.color"))
    , window_border_urgent_color("window_border_urgent_color",
                                 getColorAttr("theme.tiling.urgent.color"),
                                 setColorAttr("theme.urgent.color"))
{
    verbose = g_verbose > 0;
    verbose.changed().connect([](bool newVal) { g_verbose = newVal; });
    wireAttributes({
        &verbose,
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
        &hide_covered_windows,
        &smart_frame_surroundings,
        &smart_window_surroundings,
        &monitors_locked,
        &auto_detect_monitors,
        &auto_detect_panels,
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
        i->changed().connect([] { all_monitors_apply_layout(); });
    }
    hide_covered_windows.changed().connect([] { all_monitors_apply_layout(); });
    for (auto i : {
         &frame_border_active_color,
         &frame_border_normal_color,
         &frame_border_inner_color,
         &frame_bg_normal_color,
         &frame_bg_active_color}) {
        i->changed().connect(&reset_client_colors);
    }
    frame_bg_transparent.changed().connect(&reset_client_colors);
    for (auto i : {&frame_transparent_width,
         &frame_border_width,
         &frame_border_inner_width,
         &frame_active_opacity,
         &frame_normal_opacity}) {
        i->changed().connect(&reset_client_colors);
    }
    frame_bg_transparent.setWritable();
    for (auto i : {&always_show_frame,
         &gapless_grid,
         &smart_frame_surroundings,
         &smart_window_surroundings,
         &raise_on_focus_temporarily}) {
        i->changed().connect(&all_monitors_apply_layout);
    }
    wmname.changed().connect([]() { Ewmh::get().updateWmName(); });

    default_frame_layout.setValidator([] (size_t layout) {
        if (layout >= layoutAlgorithmCount()) {
            return "layout number must be at most "
                + to_string(layoutAlgorithmCount() - 1);
        }
        return string();
    });
    tree_style.setValidator([] (string new_value) {
        if (utf8_string_length(new_value) < 8) {
            return string("tree_style needs 8 characters");
        }
        return string();
    });
    g_settings = this;
    for (auto i : attributes()) {
        i.second->setWritable();
    }
}

void Settings::injectDependencies(Root* root) {
    root_ = root;
    // TODO: the lock level is not a setting! should move somewhere else
    monitors_locked = root->globals.initial_monitors_locked;
    monitors_locked.changed().connect([root](bool) {
        root->monitors()->lock_number_changed();
    });
}

function<int()> Settings::getIntAttr(string name) {
    return [this, name]() {
        Attribute* a = this->root_->deepAttribute(name);
        if (a) {
            try {
                return std::stoi(a->str());
            } catch (...) {
                return 0;
            }
        } else {
            HSDebug("Internal Error: No such attribute %s\n", name.c_str());
            return 0;
        }
    };
}

function<Color()> Settings::getColorAttr(string name) {
    return [this, name]() {
        Attribute* a = this->root_->deepAttribute(name);
        if (a) {
            return Color(a->str());
        } else {
            HSDebug("Internal Error: No such attribute %s\n", name.c_str());
            return Color("black");
        }
    };
}

function<string(int)> Settings::setIntAttr(string name) {
    return [this, name](int val) {
        Attribute* a = this->root_->deepAttribute(name);
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
function<string(Color)> Settings::setColorAttr(string name) {
    return [this, name](Color val) {
        Attribute* a = this->root_->deepAttribute(name);
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

int Settings::set_cmd(Input input, Output output) {
    string set_name, value;
    if (!(input >> set_name >> value)) {
        return HERBST_NEED_MORE_ARGS;
    }

    auto attr = attribute(set_name);
    if (!attr) {
        output << input.command() <<
            ": Setting \"" << set_name << "\" not found\n";
        return HERBST_SETTING_NOT_FOUND;
    }
    auto msg = attr->change(value);
    if (!msg.empty()) {
        output << input.command()
               << ": Invalid value \"" << value
               << "\" for setting \"" << set_name << "\": "
               << msg << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    return 0;
}

void Settings::set_complete(Completion& complete) {
    if (complete == 0) {
        for (auto& a : attributes()) {
            complete.full(a.first);
        }
    } else if (complete == 1) {
        Attribute* a = attribute(complete[0]);
        if (a) {
            a->complete(complete);
        }
    } else {
        complete.none();
    }
}

int Settings::toggle_cmd(Input argv, Output output) {
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
    if (attr->type() == Type::ATTRIBUTE_BOOL) {
        attr->change("toggle");
    } else {
        output << argv.command()
            << ": Setting \"" << set_name
            << "\" is not of type bool\n";
        return HERBST_INVALID_ARGUMENT;
    }
    return 0;
}

void Settings::toggle_complete(Completion& complete) {
    if (complete == 0) {
        for (auto a : attributes()) {
            if (a.second->type() == Type::ATTRIBUTE_BOOL) {
                complete.full(a.first);
            }
        }
    } else {
        complete.none();
    }
}

int Settings::cycle_value_cmd(Input argv, Output output) {
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
    if (!msg.empty()) {
        output << argv.command()
               << ": Invalid value for setting \""
               << set_name << "\": "
               << msg << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    return 0;
}

void Settings::cycle_value_complete(Completion& complete) {
    if (complete == 0) {
        for (auto a : attributes()) {
            complete.full(a.first);
        }
    } else {
        Attribute* a = attribute(complete[0]);
        if (a) {
            a->complete(complete);
        }
    }
}

int Settings::get_cmd(Input argv, Output output) {
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

void Settings::get_complete(Completion& complete) {
    if (complete == 0) {
        for (auto& a : attributes()) {
            complete.full(a.first);
        }
    } else if (complete == 1) {
        Attribute* a = attribute(complete[0]);
        if (a) {
            a->complete(complete);
        }
    } else {
        complete.none();
    }
}



