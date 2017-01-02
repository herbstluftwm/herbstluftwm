/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_MONITOR_H_
#define __HERBSTLUFT_MONITOR_H_

#include <stdbool.h>
#include <X11/Xlib.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */

#include "x11-types.h"
#include "floating.h"
#include "utils.h"
#include "object.h"

class HSTag;
class HSFrame;
struct HSSlice;
struct HSStack;

class HSMonitor : public Object {
public:
    ~HSMonitor();
    Rectangle getFloatingArea();
    int relativeX(int x_root);
    int relativeY(int y_root);

    HSTag*      tag;    // currently viewed tag
    HSTag*      tag_previous;    // previously viewed tag
    struct HSSlice*    slice;  // slice in the monitor stack
    std::string    name;
    int         pad_up;
    int         pad_right;
    int         pad_down;
    int         pad_left;
    bool        dirty;
    bool        lock_frames;
    bool        lock_tag;
    struct {
        // last saved mouse position
        int x;
        int y;
    } mouse;
    Rectangle   rect;   // area for this monitor
    Window      stacking_window;   // window used for making stacking easy
};

void monitor_init();
void monitor_destroy();

// adds a new monitor to the monitors list and returns a pointer to it
HSMonitor* monitor_with_frame(HSFrame* frame);
HSMonitor* monitor_with_coordinate(int x, int y);
HSMonitor* monitor_with_index(int index);
HSMonitor* find_monitor_with_tag(HSTag* tag);
HSMonitor* add_monitor(Rectangle rect,  HSTag* tag, char* name);
void monitor_focus_by_index(int new_selection);
int monitor_index_of(HSMonitor* monitor);
int monitor_cycle_command(int argc, char** argv);
int monitor_focus_command(int argc, char** argv, Output output);
int find_monitor_index_by_name(char* name);
HSMonitor* find_monitor_by_name(char* name);
HSMonitor* string_to_monitor(char* string);
int string_to_monitor_index(char* string);
int monitor_index_in_direction(HSMonitor* m, enum HSDirection dir);
int add_monitor_command(int argc, char** argv, Output output);
int monitor_raise_command(int argc, char** argv, Output output);
int remove_monitor_command(int argc, char** argv, Output output);
int remove_monitor(int index);
int list_monitors(int argc, char** argv, Output output);
int list_padding(int argc, char** argv, Output output);
int set_monitor_rects_command(int argc, char** argv, Output output);
int disjoin_rects_command(int argc, char** argv, Output output);
int set_monitor_rects(const RectangleVec &templates);
int move_monitor_command(int argc, char** argv, Output output);
int rename_monitor_command(int argc, char** argv, Output output);
int monitor_rect_command(int argc, char** argv, Output output);
HSMonitor* get_current_monitor();
int monitor_count();
int monitor_set_tag(HSMonitor* monitor, HSTag* tag);
int monitor_set_pad_command(int argc, char** argv, Output output);
int monitor_set_tag_command(int argc, char** argv, Output output);
int monitor_set_tag_by_index_command(int argc, char** argv, Output output);
int monitor_set_previous_tag_command(int argc, char** argv, Output output);
void monitors_lock();
void monitors_unlock();
int monitors_lock_command(int argc, const char** argv);
int monitors_unlock_command(int argc, const char** argv);
void monitors_lock_changed();
int monitor_lock_tag_command(int argc, char** argv, Output output);
int monitor_unlock_tag_command(int argc, char** argv, Output output);
void monitor_apply_layout(HSMonitor* monitor);
void all_monitors_apply_layout();
void ensure_monitors_are_available();
void all_monitors_replace_previous_tag(HSTag* old, HSTag* newmon);

void drop_enternotify_events();

void monitor_restack(HSMonitor* monitor);
int monitor_stack_window_count(bool real_clients);
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

