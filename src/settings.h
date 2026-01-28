#ifndef __HERBSTLUFT_SETTINGS_H_
#define __HERBSTLUFT_SETTINGS_H_

#include <string>

#include "attribute_.h"
#include "commandio.h"
#include "framedata.h"
#include "globals.h"
#include "object.h"
#include "x11-types.h"

class Root;
class Completion;

enum class SmartFrameSurroundings {
    hide_all,
    hide_gaps,
    off,
};

template <>
struct is_finite<SmartFrameSurroundings> : std::true_type {};
template<> Finite<SmartFrameSurroundings>::ValueList Finite<SmartFrameSurroundings>::values;
template<> inline Type Attribute_<SmartFrameSurroundings>::staticType() { return Type::NAMES; }

enum class SmartWindowSurroundings {
    one_window,
    one_window_and_frame,
    off,
};

template <>
struct is_finite<SmartWindowSurroundings> : std::true_type {};
template<> Finite<SmartWindowSurroundings>::ValueList Finite<SmartWindowSurroundings>::values;
template<> inline Type Attribute_<SmartWindowSurroundings>::staticType() { return Type::NAMES; }

enum class ShowFrameDecorations {
    none,
    nonempty,
    focused_if_multiple,
    focused,
    if_empty,
    if_multiple_empty,
    if_multiple,
    all,
};

template <>
struct is_finite<ShowFrameDecorations> : std::true_type {};
template<> Finite<ShowFrameDecorations>::ValueList Finite<ShowFrameDecorations>::values;
template<> inline Type Attribute_<ShowFrameDecorations>::staticType() { return Type::NAMES; }


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
    Attribute_<ShowFrameDecorations> show_frame_decorations = {"show_frame_decorations", ShowFrameDecorations::focused_if_multiple};
    Attribute_<bool>          default_direction_external_only = {"default_direction_external_only", false};
    Attribute_<LayoutAlgorithm> default_frame_layout = {"default_frame_layout", LayoutAlgorithm::vertical};
    Attribute_<bool>          focus_follows_mouse = {"focus_follows_mouse", false};
    Attribute_<bool>          focus_stealing_prevention = {"focus_stealing_prevention", true};
    Attribute_<bool>          swap_monitors_to_get_tag = {"swap_monitors_to_get_tag", true};
    Attribute_<bool>          raise_on_focus = {"raise_on_focus", false};
    Attribute_<bool>          raise_on_focus_temporarily = {"raise_on_focus_temporarily", false};
    Attribute_<bool>          raise_on_click = {"raise_on_click", true};
    Attribute_<bool>          gapless_grid = {"gapless_grid", true};
    Attribute_<bool>          tabbed_max = {"tabbed_max", true};
    Attribute_<bool>          max_tab_reorder = {"max_tab_reorder", false};
    Attribute_<bool>          hide_covered_windows = {"hide_covered_windows", false};
    Attribute_<SmartFrameSurroundings> smart_frame_surroundings = {"smart_frame_surroundings", SmartFrameSurroundings::off};
    Attribute_<SmartWindowSurroundings> smart_window_surroundings = {"smart_window_surroundings", SmartWindowSurroundings::off};
    Attribute_<unsigned long> monitors_locked = {"monitors_locked", 0};
    Attribute_<bool>          auto_detect_monitors = {"auto_detect_monitors", false};
    Attribute_<bool>          auto_detect_panels = {"auto_detect_panels", true};
    Attribute_<int>           pseudotile_center_threshold = {"pseudotile_center_threshold", 10};
    Attribute_<bool>          update_dragged_clients = {"update_dragged_clients", false};
    Attribute_<string>        ellipsis = {"ellipsis", "..."};
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
