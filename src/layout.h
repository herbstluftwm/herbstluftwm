/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_LAYOUT_H_
#define __HERBSTLUFT_LAYOUT_H_

#include <glib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <X11/Xlib.h>

#define LAYOUT_DUMP_BRACKETS "()" /* must consist of exactly two chars */
#define LAYOUT_DUMP_WHITESPACES " \t\n" /* must be at least one char */
#define LAYOUT_DUMP_SEPARATOR_STR ":" /* must be a string with one char */
#define LAYOUT_DUMP_SEPARATOR LAYOUT_DUMP_SEPARATOR_STR[0]

#define TAG_SET_FLAG(tag, flag) \
    ((tag)->flags |= (flag))
enum {
    TAG_FLAG_URGENT = 0x01, // is there a urgent window?
    TAG_FLAG_USED   = 0x02, // the oposit of empty
};

enum {
    ALIGN_VERTICAL = 0,
    ALIGN_HORIZONTAL,
};

enum {
    LAYOUT_VERTICAL = 0,
    LAYOUT_HORIZONTAL,
    LAYOUT_MAX,
    LAYOUT_GRID,
    LAYOUT_COUNT,
};

extern char* g_align_names[];
extern char* g_layout_names[];

enum {
    TYPE_CLIENTS = 0,
    TYPE_FRAMES,
};

// execute an action on an client
// returns Success or failure.
struct HSClient;
typedef int (*ClientAction)(struct HSClient*, void* data);

#define FRACTION_UNIT 10000

struct HSFrame;
struct HSTag;

typedef struct HSLayout {
    int align;         // ALIGN_VERTICAL or ALIGN_HORIZONTAL
    struct HSFrame* a; // first child
    struct HSFrame* b; // second child
    int selection;
    int fraction; // size of first child relative to whole size
                  // FRACTION_UNIT means full size
                  // FRACTION_UNIT/2 means 50%
} HSLayout;

typedef struct HSFrame {
    union {
        HSLayout layout;
        struct {
            Window* buf;
            size_t  count;
            int     selection;
            int     layout;
        } clients;
    } content;
    int type;
    struct HSFrame* parent;
    Window window;
    bool   window_visible;
} HSFrame;


typedef struct HSMonitor {
    struct HSTag*      tag;    // currently viewed tag
    int         pad_up;
    int         pad_right;
    int         pad_down;
    int         pad_left;
    struct {
        // last saved mouse position
        int x;
        int y;
    } mouse;
    XRectangle  rect;   // area for this monitor
} HSMonitor;

typedef struct HSTag {
    GString*    name;   // name of this tag
    HSFrame*    frame;  // the master frame
    bool        floating;
    int         flags;
} HSTag;

// globals
GArray*     g_tags; // Array of HSTag*
GArray*     g_monitors; // Array of HSMonitor
int         g_cur_monitor;
HSFrame*    g_cur_frame; // currently selected frame
bool        g_tag_flags_dirty;

// functions
void layout_init();
void layout_destroy();
// for frames
HSFrame* frame_create_empty();
void frame_insert_window(HSFrame* frame, Window window);
void frame_insert_window_at_index(HSFrame* frame, Window window, char* index);
HSFrame* frame_current_selection();
// removes window from a frame/subframes
// returns true, if window was found. else: false
bool frame_remove_window(HSFrame* frame, Window window);
// destroys a frame and all its childs
// then all Windows in it are collected and returned
// YOU have to g_free the resulting window-buf
void frame_destroy(HSFrame* frame, Window** buf, size_t* count);
void frame_split(HSFrame* frame, int align, int fraction);
int frame_split_command(int argc, char** argv);
int frame_change_fraction_command(int argc, char** argv);

void frame_apply_layout(HSFrame* frame, XRectangle rect);
void reset_frame_colors();
HSFrame* get_toplevel_frame(HSFrame* frame);

void print_tag_tree(HSTag* tag, GString** output);
void print_frame_tree(HSFrame* frame, char* indent, char* rootprefix, GString** output);
void dump_frame_tree(HSFrame* frame, GString** output);
// create apply a described layout to a frame and its subframes
// returns pointer to string that wasnot parsed yet
// or NULL on an error
char* load_frame_tree(HSFrame* frame, char* description, GString** errormsg);
int find_layout_by_name(char* name);
int find_align_by_name(char* name);

int frame_current_cycle_selection(int argc, char** argv);
int cycle_all_command(int argc, char** argv);

void frame_unfocus(); // unfocus currently focused window

// get neighbour in a specific direction 'l' 'r' 'u' 'd' (left, right,...)
// returns the neighbour or NULL if there is no one
HSFrame* frame_neighbour(HSFrame* frame, char direction);
int frame_inner_neighbour_index(HSFrame* frame, char direction);
int frame_focus_command(int argc, char** argv);

// follow selection to leave and focus this frame
int frame_focus_recursive(HSFrame* frame);
void frame_do_recursive(HSFrame* frame, void (*action)(HSFrame*), int order);
void frame_hide_recursive(HSFrame* frame);
void frame_show_recursive(HSFrame* frame);
// do an action for each client in frame tree
// returns success or failure
int frame_foreach_client(HSFrame* frame, ClientAction action, void* data);

void frame_apply_client_layout_linear(HSFrame* frame, XRectangle rect, bool vertical);
void frame_apply_client_layout_horizontal(HSFrame* frame, XRectangle rect);
void frame_apply_client_layout_vertical(HSFrame* frame, XRectangle rect);
int frame_current_cycle_client_layout(int argc, char** argv);
int frame_current_set_client_layout(int argc, char** argv);
int frame_split_count_to_root(HSFrame* frame, int align);

// returns the Window that is focused
// returns 0 if there is none
Window frame_focused_window(HSFrame* frame);
bool frame_focus_window(HSFrame* frame, Window win);
bool focus_window(Window win, bool switch_tag, bool switch_monitor);
// moves a window to an other frame
int frame_move_window_command(int argc, char** argv);
/// removes the current frame
int frame_remove_command(int argc, char** argv);
void frame_set_visible(HSFrame* frame, bool visible);

// for tags
HSTag* add_tag(char* name);
HSTag* find_tag(char* name);
int    tag_index_of(HSTag* tag);
HSTag* find_unused_tag();
HSTag* find_tag_with_toplevel_frame(HSFrame* frame);
int tag_add_command(int argc, char** argv);
int tag_rename_command(int argc, char** argv);
int tag_move_window_command(int argc, char** argv);
void tag_move_window(HSTag* target);
int tag_remove_command(int argc, char** argv);
int tag_set_floating_command(int argc, char** argv, GString** result);
void tag_force_update_flags();
void tag_update_flags();
void tag_set_flags_dirty();
// for monitors
// adds a new monitor to g_monitors and returns a pointer to it
HSMonitor* monitor_with_frame(HSFrame* frame);
HSMonitor* monitor_with_coordinate(int x, int y);
HSMonitor* monitor_with_index(int index);
HSMonitor* find_monitor_with_tag(HSTag* tag);
HSMonitor* add_monitor(XRectangle rect, HSTag* tag);
void monitor_focus_by_index(int new_selection);
int monitor_get_relative_x(HSMonitor* m, int x_root);
int monitor_get_relative_y(HSMonitor* m, int y_root);
int monitor_index_of(HSMonitor* monitor);
int monitor_cycle_command(int argc, char** argv);
int monitor_focus_command(int argc, char** argv);
int add_monitor_command(int argc, char** argv);
int remove_monitor_command(int argc, char** argv);
int list_monitors(int argc, char** argv, GString** output);
int move_monitor_command(int argc, char** argv);
int monitor_rect_command(int argc, char** argv, GString** result);
HSMonitor* get_current_monitor();
void monitor_set_tag(HSMonitor* monitor, HSTag* tag);
int monitor_set_pad_command(int argc, char** argv);
int monitor_set_tag_command(int argc, char** argv);
int monitor_set_tag_by_index_command(int argc, char** argv);
void monitor_apply_layout(HSMonitor* monitor);
void all_monitors_apply_layout();
void ensure_monitors_are_available();

#endif


