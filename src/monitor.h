/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_MONITOR_H_
#define __HERBSTLUFT_MONITOR_H_

#include <glib.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */

struct HSTag;
struct HSFrame;

typedef struct HSMonitor {
    struct HSTag*      tag;    // currently viewed tag
    int         pad_up;
    int         pad_right;
    int         pad_down;
    int         pad_left;
    bool        dirty;
    struct {
        // last saved mouse position
        int x;
        int y;
    } mouse;
    XRectangle  rect;   // area for this monitor
} HSMonitor;

// globals
GArray*     g_monitors; // Array of HSMonitor
int         g_cur_monitor;

void monitor_init();
void monitor_destroy();

// adds a new monitor to g_monitors and returns a pointer to it
HSMonitor* monitor_with_frame(struct HSFrame* frame);
HSMonitor* monitor_with_coordinate(int x, int y);
HSMonitor* monitor_with_index(int index);
HSMonitor* find_monitor_with_tag(struct HSTag* tag);
HSMonitor* add_monitor(XRectangle rect, struct HSTag* tag);
void monitor_focus_by_index(int new_selection);
int monitor_get_relative_x(HSMonitor* m, int x_root);
int monitor_get_relative_y(HSMonitor* m, int y_root);
int monitor_index_of(HSMonitor* monitor);
int monitor_cycle_command(int argc, char** argv);
int monitor_focus_command(int argc, char** argv);
int add_monitor_command(int argc, char** argv);
int remove_monitor_command(int argc, char** argv);
int remove_monitor(int index);
int list_monitors(int argc, char** argv, GString** output);
int set_monitor_rects_command(int argc, char** argv, GString** output);
int disjoin_rects_command(int argc, char** argv, GString** output);
int set_monitor_rects(XRectangle* templates, size_t count);
int move_monitor_command(int argc, char** argv);
int monitor_rect_command(int argc, char** argv, GString** result);
HSMonitor* get_current_monitor();
void monitor_set_tag(HSMonitor* monitor, struct HSTag* tag);
int monitor_set_pad_command(int argc, char** argv);
int monitor_set_tag_command(int argc, char** argv);
int monitor_set_tag_by_index_command(int argc, char** argv);
int monitors_lock_command(int argc, char** argv);
int monitors_unlock_command(int argc, char** argv);
void monitors_lock_changed();
void monitor_apply_layout(HSMonitor* monitor);
void all_monitors_apply_layout();
void ensure_monitors_are_available();

typedef bool (*MonitorDetection)(XRectangle**, size_t*);
bool detect_monitors_xinerama(XRectangle** ret_rects, size_t* ret_count);
bool detect_monitors_simple(XRectangle** ret_rects, size_t* ret_count);
int detect_monitors_command(int argc, char **argv);

#endif

