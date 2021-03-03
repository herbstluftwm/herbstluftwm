#ifndef __HERBSTLUFT_SETTINGS_H_
#define __HERBSTLUFT_SETTINGS_H_

#include <string>

#include "attribute_.h"
#include "commandio.h"
#include "globals.h"
#include "object.h"
#include "x11-types.h"

class Root;
class Completion;

class Settings : public Object {
public:
    using string = std::string;
    Settings();
    void injectDependencies(Root* root);
    // commands:
    int set_cmd(Input input, Output output);
    void set_complete(Completion& complete);
    int get_cmd(Input argv, Output output);
    void get_complete(Completion& complete);
    int toggle_cmd(Input argv, Output output);
    void toggle_complete(Completion& complete);
    int cycle_value_cmd(Input argv, Output output);
    void cycle_value_complete(Completion& complete);

    // all the settings:
    Attribute_<bool>          verbose = {"verbose", false};
    Attribute_<int>           frame_gap = {"frame_gap", 5};
    Attribute_<int>           frame_padding = {"frame_padding", 0};
    Attribute_<int>           window_gap = {"window_gap", 0};
    Attribute_<int>           snap_distance = {"snap_distance", 10};
    Attribute_<int>           snap_gap = {"snap_gap", 5};
    Attribute_<int>           mouse_recenter_gap = {"mouse_recenter_gap", 0};
    Attribute_<Color>         frame_border_active_color = {"frame_border_active_color", {"red"}};
    Attribute_<Color>         frame_border_normal_color = {"frame_border_normal_color", {"blue"}};
    Attribute_<Color>         frame_border_inner_color = {"frame_border_inner_color", {"black"}};
    Attribute_<Color>         frame_bg_normal_color = {"frame_bg_normal_color", {"black"}};
    Attribute_<Color>         frame_bg_active_color = {"frame_bg_active_color", {"black"}};
    Attribute_<bool>          frame_bg_transparent = {"frame_bg_transparent", false};
    Attribute_<int>           frame_transparent_width = {"frame_transparent_width", 0};
    Attribute_<int>           frame_border_width = {"frame_border_width", 2};
    Attribute_<int>           frame_border_inner_width = {"frame_border_inner_width", 0};
    Attribute_<int>           frame_active_opacity = {"frame_active_opacity", 100};
    Attribute_<int>           frame_normal_opacity = {"frame_normal_opacity", 100};
    Attribute_<bool>          focus_crosses_monitor_boundaries = {"focus_crosses_monitor_boundaries", true};
    Attribute_<bool>          always_show_frame = {"always_show_frame", false};
    Attribute_<bool>          default_direction_external_only = {"default_direction_external_only", false};
    Attribute_<unsigned long> default_frame_layout = {"default_frame_layout", 0};
    Attribute_<bool>          focus_follows_mouse = {"focus_follows_mouse", false};
    Attribute_<bool>          focus_stealing_prevention = {"focus_stealing_prevention", true};
    Attribute_<bool>          swap_monitors_to_get_tag = {"swap_monitors_to_get_tag", true};
    Attribute_<bool>          raise_on_focus = {"raise_on_focus", false};
    Attribute_<bool>          raise_on_focus_temporarily = {"raise_on_focus_temporarily", false};
    Attribute_<bool>          raise_on_click = {"raise_on_click", true};
    Attribute_<bool>          gapless_grid = {"gapless_grid", true};
    Attribute_<bool>          hide_covered_windows = {"hide_covered_windows", false};
    Attribute_<bool>          smart_frame_surroundings = {"smart_frame_surroundings", false};
    Attribute_<bool>          smart_window_surroundings = {"smart_window_surroundings", false};
    Attribute_<unsigned long> monitors_locked = {"monitors_locked", 0};
    Attribute_<bool>          auto_detect_monitors = {"auto_detect_monitors", false};
    Attribute_<bool>          auto_detect_panels = {"auto_detect_panels", true};
    Attribute_<int>           pseudotile_center_threshold = {"pseudotile_center_threshold", 10};
    Attribute_<bool>          update_dragged_clients = {"update_dragged_clients", false};
    Attribute_<string>        tree_style = {"tree_style", "*| +`--."};
    Attribute_<string>        wmname = {"wmname", WINDOW_MANAGER_NAME};
    // for compatibility
    DynAttribute_<int>         window_border_width;
    DynAttribute_<int>         window_border_inner_width;
    DynAttribute_<Color>       window_border_inner_color;
    DynAttribute_<Color>       window_border_active_color;
    DynAttribute_<Color>       window_border_normal_color;
    DynAttribute_<Color>       window_border_urgent_color;
private:
    std::function<int()> getIntAttr(std::string name);
    std::function<Color()> getColorAttr(std::string name);
    std::function<string(int)> setIntAttr(std::string name);
    std::function<string(Color)> setColorAttr(std::string name);
    Root* root_ = nullptr;
};

extern Settings* g_settings;

#endif

