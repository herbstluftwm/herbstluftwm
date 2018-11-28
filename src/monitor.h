#ifndef __HERBSTLUFT_MONITOR_H_
#define __HERBSTLUFT_MONITOR_H_

#include <X11/Xlib.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */

#include "x11-types.h"
#include "floating.h"
#include "types.h"
#include "object.h"
#include "attribute_.h"

class HSTag;
class HSFrame;
class Settings;
class MonitorManager;
struct HSSlice;
struct HSStack;

class HSMonitor : public Object {
public:
    HSMonitor(Settings* settings, MonitorManager* monman, Rectangle Rect, HSTag* tag);
    ~HSMonitor() override;
    Rectangle getFloatingArea();
    int relativeX(int x_root);
    int relativeY(int y_root);

    HSTag*      tag;    // currently viewed tag
    HSTag*      tag_previous;    // previously viewed tag
    struct HSSlice*    slice;  // slice in the monitor stack
    Attribute_<std::string>   name = {"name", {}};
    Attribute_<unsigned long> index = {"index", 0};
    DynAttribute_<std::string>   tag_string;
    Attribute_<int>         pad_up = {"pad_up", 0};
    Attribute_<int>         pad_right = {"pad_right", 0};
    Attribute_<int>         pad_down = {"pad_down", 0};
    Attribute_<int>         pad_left = {"pad_left", 0};
    bool        dirty;
    bool        lock_frames;
    Attribute_<bool>        lock_tag;
    struct {
        // last saved mouse position
        int x;
        int y;
    } mouse;
    Rectangle   rect;   // area for this monitor
    Window      stacking_window;   // window used for making stacking easy
    void setIndexAttribute(unsigned long index) override;
    int lock_tag_cmd(Input argv, Output output);
    int unlock_tag_cmd(Input argv, Output output);
    int list_padding(Input input, Output output);
    int move_cmd(Input input, Output output);
    bool setTag(HSTag* new_tag);
    void applyLayout();
    void restack();
private:
    std::string getTagString();
    std::string setTagString(std::string new_tag);
    Settings* settings;
    MonitorManager* monman;
};

void monitor_init();
void monitor_destroy();

class MonitorManager;
extern MonitorManager* g_monitors; // temporarily
extern int g_cur_monitor; // temporarily

// adds a new monitor to the monitors list and returns a pointer to it
HSMonitor* monitor_with_coordinate(int x, int y);
HSMonitor* find_monitor_with_tag(HSTag* tag);
void monitor_focus_by_index(unsigned new_selection);
int monitor_cycle_command(int argc, char** argv);
int monitor_focus_command(int argc, char** argv, Output output);
HSMonitor* find_monitor_by_name(const char* name);
HSMonitor* string_to_monitor(char* string);
int string_to_monitor_index(char* string);
int monitor_raise_command(int argc, char** argv, Output output);
int remove_monitor_command(int argc, char** argv, Output output);
int remove_monitor(int index);
int set_monitor_rects_command(int argc, char** argv, Output output);
int set_monitor_rects(const RectangleVec &templates);
int rename_monitor_command(int argc, char** argv, Output output);
int monitor_rect_command(int argc, char** argv, Output output);
HSMonitor* get_current_monitor();
int monitor_set_tag(HSMonitor* monitor, HSTag* tag);
int monitor_set_pad_command(int argc, char** argv, Output output);
int monitor_set_tag_command(int argc, char** argv, Output output);
int monitor_set_tag_by_index_command(int argc, char** argv, Output output);
int monitor_set_previous_tag_command(int argc, char** argv, Output output);
void all_monitors_apply_layout();
void ensure_monitors_are_available();
void all_monitors_replace_previous_tag(HSTag* old, HSTag* newmon);

void drop_enternotify_events();

void monitor_stack_to_window_buf(Window* buf, int len, bool real_clients,
                                 int* remain_len);
struct HSStack* get_monitor_stack();

void monitor_update_focus_objects();

typedef bool (*MonitorDetection)(RectangleVec &);
bool detect_monitors_xinerama(RectangleVec &dest);
bool detect_monitors_simple(RectangleVec &dest);
int detect_monitors_command(int argc, const char **argv, Output output);

int shift_to_monitor(int argc, char** argv, Output output);

#endif

