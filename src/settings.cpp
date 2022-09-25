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

template<>
Finite<SmartFrameSurroundings>::ValueList Finite<SmartFrameSurroundings>::values = ValueListPlain {
    { SmartFrameSurroundings::hide_all, "hide_all" },
    { SmartFrameSurroundings::hide_all, "on" },
    { SmartFrameSurroundings::hide_all, "true" },
    { SmartFrameSurroundings::hide_all, "1" },
    { SmartFrameSurroundings::hide_gaps, "hide_gaps" },
    { SmartFrameSurroundings::off, "off" },
    { SmartFrameSurroundings::off, "false" },
    { SmartFrameSurroundings::off, "0" },
};

template<>
Finite<ShowFrameDecorations>::ValueList Finite<ShowFrameDecorations>::values = ValueListPlain {
    { ShowFrameDecorations::all, "all" },
    { ShowFrameDecorations::focused, "focused" },
    { ShowFrameDecorations::focused_if_multiple, "focused_if_multiple" },
    { ShowFrameDecorations::if_multiple, "if_multiple" },
    { ShowFrameDecorations::nonempty, "nonempty" },
    { ShowFrameDecorations::if_empty, "if_empty" },
    { ShowFrameDecorations::none, "none" },
};


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
        &show_frame_decorations,
        &default_direction_external_only,
        &default_frame_layout,
        &focus_follows_mouse,
        &focus_stealing_prevention,
        &swap_monitors_to_get_tag,
        &raise_on_focus,
        &raise_on_focus_temporarily,
        &raise_on_click,
        &gapless_grid,
        &tabbed_max,
        &hide_covered_windows,
        &smart_frame_surroundings,
        &smart_window_surroundings,
        &monitors_locked,
        &auto_detect_monitors,
        &auto_detect_panels,
        &pseudotile_center_threshold,
        &update_dragged_clients,
        &ellipsis,
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
    ellipsis.changed().connect([] { all_monitors_apply_layout(); });
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
    for (auto i : {&gapless_grid,
         &tabbed_max,
         &smart_window_surroundings,
         &raise_on_focus_temporarily}) {
        i->changed().connect(&all_monitors_apply_layout);
    }
    show_frame_decorations.changed().connect(&all_monitors_apply_layout);
    smart_frame_surroundings.changed().connect(&all_monitors_apply_layout);
    wmname.changed().connect([]() { Ewmh::get().updateWmName(); });
    // connect deprecated attribute to new settings:
    always_show_frame.changedByUser().connect([this](bool alwaysShow) {
        this->show_frame_decorations =
                alwaysShow
                ? ShowFrameDecorations::all
                : ShowFrameDecorations::focused;
    });
    show_frame_decorations.changed().connect([this](ShowFrameDecorations newValue) {
        this->always_show_frame = newValue == ShowFrameDecorations::all;
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
    setDoc(
        "Settings configure the general behaviour of herbstluftwm and can be "
        "controlled via the \'set\', \'get\' and \'toggle\' commands. "
        "The settings. object has an attribute for each setting. Many settings "
        "are wrappers around attributes and only remain for compatibility."
    );
    frame_gap.setDoc("The gap between frames in the tiling mode.");
    frame_padding.setDoc("The padding within a frame in the tiling "
                         "mode, i.e. the space between the border of "
                         "a frame and the windows within it.");
    window_gap.setDoc("The gap between windows within one frame in "
                      "the tiling mode.");
    snap_distance.setDoc(
                "If a client is dragged in floating mode, then it snaps "
                "to neighbour clients if the distance between them is "
                "smaller than snap_distance.");
    snap_gap.setDoc(
                "Specifies the remaining gap if a dragged client snaps "
                "to an edge in floating mode. If snap_gap is set to 0, "
                "no gap will remain.");
    mouse_recenter_gap.setDoc(
                "Specifies the gap around a monitor. If the monitor is "
                "selected and the mouse position would be restored into "
                "this gap, it is set to the center of the monitor. This "
                "is useful, when the monitor was left via mouse movement, "
                "but is reselected by keyboard. If the gap is 0 (default), "
                "the mouse is never recentered.");
    frame_border_active_color.setDoc("The border color of a focused frame.");
    frame_border_normal_color.setDoc("The border color of an unfocused frame.");
    frame_border_inner_color.setDoc("The color of the inner border of a frame.");
    frame_bg_active_color.setDoc("The fill color of a focused frame.");
    frame_bg_normal_color.setDoc("The fill color of an unfocused frame (It is "
                                 "only visible if non-focused frames are configured "
                                 "to be visible, see \'show_frame_decorations\').");
    frame_bg_transparent.setDoc(
                "If set, the background of frames are transparent. That means "
                "a rectangle is cut out from the inner such that only the "
                "frame border and a stripe of width \'frame_transparent_width\' "
                "can be seen. Use \'frame_active_opacity\' and "
                "\'frame_normal_opacity\' for real transparency.");
    frame_transparent_width.setDoc(
                "Specifies the width of the remaining frame colored with "
                "\'frame_bg_active_color\' if \'frame_bg_transparent\' is set.");
    frame_border_width.setDoc("Border width of a frame.");
    frame_border_inner_width.setDoc(
                "The width of the inner border of a frame. Must be less than "
                "\'frame_border_width\', since it does not add to the frame "
                "border width but is a part of it.");
    focus_crosses_monitor_boundaries.setDoc(
                "If set, commands +focus+ and +shift+ cross monitor boundaries. "
                "If there is no client in the direction given to +focus+, then "
                "the monitor in the specified direction is focused. Similarly, "
                "if +shift+ cannot move a window within a tag, the window is "
                "moved to the neighbour monitor in the desired direction.");

    raise_on_focus.setDoc(
                "If set, a window is raised if it is focused. The value of "
                "this setting is only used in floating mode.");
    raise_on_focus_temporarily.setDoc(
                "If set, a window is raised temporarily if it is focused on "
                "its tag. Temporarily in this case means that the window will "
                "return to its previous stacking position if another window "
                "is focused.");
    raise_on_click.setDoc(
                "If set, a window is raised if it is clicked. The value of "
                "this setting is only noticed in floating mode.");

    window_border_width.setDoc(
                "Border width of a window."
                "\n\n"
                "*Warning:* This only exists for compatibility reasons; "
                "it is only an alias for the attribute +theme.border_width+.");

    window_border_inner_width.setDoc(
                "The width of the inner border of a window. Must be less than "
                "window_border_width, since it does not add to the window "
                "border width but is a part of it."
                "\n\n"
                "*Warning:* This only exists for compatibility reasons; it "
                "is only an alias for the attribute +theme.inner_width+.");

    window_border_active_color.setDoc(
                "Border color of a focused window."
                "\n\n"
                "*Warning:* This only exists for compatibility reasons; "
                "it is only an alias for the attribute +theme.active.color+.");

    window_border_normal_color.setDoc(
                "Border color of an unfocused window."
                "\n\n"
                "*Warning:* This only exists for compatibility reasons; "
                "it is only an alias for the attribute +theme.normal.color+.");

    window_border_urgent_color.setDoc(
                "Border color of an unfocused but urgent window. "
                "\n\n"
                "*Warning:* This only exists for compatibility reasons; it "
                "is only an alias for the attribute +theme.urgent.color+.");

    window_border_inner_color.setDoc(
                "Color of the inner border of a window. "
                "*Warning:* This only exists for compatibility reasons; "
                "it is only an alias for the attribute +theme.inner_color+.");


    always_show_frame.setDoc(
                "DEPRECATED, use +show_frame_decorations+ instead. Setting "
                "this corresponds to \'focused\' in \'show_frame_decorations\'."
                );

    show_frame_decorations.setDoc(
                "This controls, which frame decorations are shown at all. \n"
                "- \'none\' shows no frame decorations at all, \n"
                "- \'nonempty\' shows decorations of frames that have client windows, \n"
                "- \'if_multiple\' shows decorations on the tags with at least two frames, \n"
                "- \'if_empty\' shows decorations of frames that have no client windows, \n"
                "- \'focused\' shows the decoration of focused and nonempty frames, \n"
                "- \'focused_if_multiple\' shows decorations of focused and non-empty frames on tags with at least two frames.\n"
                "- \'all\' shows all frame decorations."
                );

    frame_active_opacity.setDoc(
                "Focused frame opacity in percent. Requires a running "
                "compositing manager to take actual effect.");
    frame_normal_opacity.setDoc(
                "Unfocused frame opacity in percent. Requires a running "
                "compositing manager to take actual effect.");

    default_frame_layout.setDoc(
                "Name of the layout algorithm, which is used if a new "
                "frame is created (on a new tag or by a non-trivial split). "
                "See above for the <<LIST_LAYOUT_ALGORITHMS,"
                "list of layout algorithms>>.");
    default_direction_external_only.setDoc(
                "This setting controls the behaviour of focus and shift "
                "if no '-e' or '-i' argument is given. "
                "If set, then focus and shift changes the focused frame "
                "even if there are other clients in this frame in the "
                "specified \'DIRECTION\'. Else, a client within current "
                "frame is selected if it is in the specified 'DIRECTION'.");


    gapless_grid.setDoc(
                "This setting affects the size of the last client in a frame "
                "that is arranged by grid layout. If set, then the last "
                "client always fills the gap within this frame. If unset, "
                "then the last client has the same size as all other clients "
                "in this frame.");

    hide_covered_windows.setDoc(
                "If activated, windows are explicitly hidden when they are "
                "covered by another window in a frame with max layout. This "
                "only has a visible effect if a compositor is used. "
                "If activated, shadows do not stack up and transparent windows "
                "show the wallpaper behind them instead of the other clients "
                "in the max layout.");

    smart_frame_surroundings.setDoc(
                "If set to \'hide_all\', frame borders and gaps will be removed "
                "when there is no ambiguity regarding the focused frame. "
                "If set to \'hide_gaps\', only frame gaps will be removed when "
                "there is no ambiguity regarding the focused frame. "
                "Turn \'off\' to always show frame borders and gaps.");

    smart_window_surroundings.setDoc(
                "If set, window borders and gaps will be removed and minimal "
                "when there\'s no ambiguity regarding the focused window. "
                "This minimal window decoration can be configured by the "
                "+theme.minimal+ object.");

    focus_follows_mouse.setDoc(
                "If set and a window is focused by mouse cursor, this window "
                "is focused (this feature is also known as sloppy focus). "
                "If unset, you need to click to change the window focus by "
                "mouse."
                "\n\n"
                "If another window is hidden by the focus change (e.g. "
                "when having pseudotiled windows in the max layout) "
                "then an extra click is required to change the focus.");

    focus_stealing_prevention.setDoc(
                "If set, only pagers and taskbars are allowed to change "
                "the focus. If unset, all applications can request a "
                "focus change.");

    monitors_locked.setDoc(
                "If greater than 0, then the clients on all monitors aren\'t "
                "moved or resized anymore. If it is set to 0, then the "
                "arranging of monitors is enabled again, and all monitors "
                "are rearranged if their content has changed in the meantime. "
                "You should not change this setting manually due to "
                "concurrency issues; use the commands *lock* and *unlock* "
                "instead.");

    swap_monitors_to_get_tag.setDoc(
                "If set: If you want to view a tag, that already is viewed "
                "on another monitor, then the monitor contents will be "
                "swapped and you see the wanted tag on the focused monitor. "
                "If not set, the other monitor is focused if it shows the "
                "desired tag.");

    auto_detect_monitors.setDoc(
                "If set, detect_monitors is automatically executed every time "
                "a monitor is connected, disconnected or resized.");

    auto_detect_panels.setDoc(
                "If set, EWMH panels are automatically detected and reserve "
                "space at the side of the monitors they are on (via pad "
                "attributes of each monitor). "
                "This setting is activated per default.");

    tree_style.setDoc(
                "It contains the chars that are used to print a nice ascii "
                "tree. It must contain at least 8 characters. "
                "e.g. ++X|:&#35;+&#42;-.++ produces a tree like:"
                "\n\n"
                "    X-.\n"
                "      #-. child 0\n"
                "      | #-* child 00\n"
                "      | +-* child 01\n"
                "      +-. child 1\n"
                "      : #-* child 10\n"
                "      : +-* child 11\n"
                "\n\n"
                "Useful values for 'tree_style' are: "
                "+╾│ ├└╼─┐+ or +-| |&#39;--.+ or +╾│ ├╰╼─╮+."
                );

    wmname.setDoc(
                "It controls the value of the +_NET_WM_NAME+ property on "
                "the root window, which specifies the name of the running "
                "window manager. The value of this setting is not updated "
                "if the actual +_NET_WM_NAME+ property on the root window "
                "is changed externally. Example usage:"
                "\n\n"
                "  * +cycle_value wmname herbstluftwm LG3D+"
                );

    pseudotile_center_threshold.setDoc(
                "If greater than 0, it specifies the least distance between "
                "a centered pseudotile window and the border of the frame or "
                "tile it is assigned to. If this distance is lower than "
                "\'pseudotile_center_threshold\', it is aligned to the top "
                "left of the client's tile.");

    update_dragged_clients.setDoc(
                "If set, a client\'s window content is resized immediately "
                "during resizing it with the mouse. If unset, the client\'s "
                "content is resized after the mouse button is released.");

    verbose.setDoc(
                "If set, verbose output is logged to herbstluftwm\'s stderr. "
                "The default value is controlled by the *--verbose* command "
                "line flag.");

    tabbed_max.setDoc(
        "if activated, multiple windows in a frame with the \'max\' "
        "layout algorithm are drawn as tabs."
    );
    ellipsis.setDoc(
        "string to append when window or tab titles are shortened "
        "to fit in the available space."
    );
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
        output.perror() << "Setting \"" << set_name << "\" not found\n";
        return HERBST_SETTING_NOT_FOUND;
    }
    auto msg = attr->change(value);
    if (!msg.empty()) {
        output.perror()
               << "Invalid value \"" << value
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
        output.perror() << "Setting \"" << set_name << "\" not found\n";
        return HERBST_SETTING_NOT_FOUND;
    }
    if (attr->type() == Type::BOOL) {
        attr->change("toggle");
    } else {
        output.perror()
            << "Setting \"" << set_name
            << "\" is not of type bool\n";
        return HERBST_INVALID_ARGUMENT;
    }
    return 0;
}

void Settings::toggle_complete(Completion& complete) {
    if (complete == 0) {
        for (auto a : attributes()) {
            if (a.second->type() == Type::BOOL) {
                complete.full(a.first);
            }
        }
    } else {
        complete.none();
    }
}

int Settings::get_cmd(Input argv, Output output) {
    if (argv.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto attr = attribute(argv.front());
    if (!attr) {
        output.perror() << "Setting \"" << argv.front() << "\" not found\n";
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
