/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_LAYOUT_H_
#define __HERBSTLUFT_LAYOUT_H_

#include "glib-backports.h"
#include "utils.h"
#include <stdbool.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include "monitor.h"
#include "tag.h"
#include "floating.h"

#define LAYOUT_DUMP_BRACKETS "()" /* must consist of exactly two chars */
#define LAYOUT_DUMP_WHITESPACES " \t\n" /* must be at least one char */
#define LAYOUT_DUMP_SEPARATOR_STR ":" /* must be a string with one char */
#define LAYOUT_DUMP_SEPARATOR LAYOUT_DUMP_SEPARATOR_STR[0]

#define TAG_SET_FLAG(tag, flag) \
    ((tag)->flags |= (flag))

enum {
    TAG_FLAG_URGENT = 0x01, // is there a urgent window?
    TAG_FLAG_USED   = 0x02, // the opposite of empty
};

enum {
    ALIGN_VERTICAL = 0,
    ALIGN_HORIZONTAL,
    // temporary values in split_command
    ALIGN_EXPLODE,
};

enum {
    LAYOUT_VERTICAL = 0,
    LAYOUT_HORIZONTAL,
    LAYOUT_MAX,
    LAYOUT_GRID,
    LAYOUT_COUNT,
};

extern const char* g_align_names[];
extern const char* g_layout_names[];

enum {
    TYPE_CLIENTS = 0,
    TYPE_FRAMES,
};

// execute an action on an client
// returns Success or failure.
class HSClient;
typedef int (*ClientAction)(HSClient*, void* data);

#define FRACTION_UNIT 10000

struct HSFrame;
struct HSSlice;
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
            HSClient** buf;
            size_t  count;
            int     selection;
            int     layout;
        } clients;
    } content;
    int type;
    struct HSFrame* parent;
    struct HSTag*   tag;
    struct HSSlice* slice;
    Window window;
    int    window_transparent;
    bool   window_visible;
    herbstluft::Rectangle  last_rect; // last rectangle when being drawn
} HSFrame;


// globals
extern HSFrame*    g_cur_frame; // currently selected frame
extern int* g_frame_gap;
extern int* g_window_gap;
extern char* g_tree_style;

// functions
void layout_init();
void layout_destroy();
// for frames
HSFrame* frame_create_empty(HSFrame* parent, HSTag* parenttag);
void frame_insert_client(HSFrame* frame, HSClient* client);
HSFrame* lookup_frame(HSFrame* root, const char* path);
HSFrame* frame_current_selection();
HSFrame* frame_current_selection_below(HSFrame* frame);
// finds the subframe of frame that contains the window
HSFrame* find_frame_with_client(HSFrame* frame, HSClient* client);
// removes window from a frame/subframes
// returns true, if window was found. else: false
bool frame_remove_client(HSFrame* frame, HSClient* client);
// destroys a frame and all its childs
// then all Windows in it are collected and returned
// YOU have to g_free the resulting window-buf
void frame_destroy(HSFrame* frame, HSClient*** buf, size_t* count);
bool frame_split(HSFrame* frame, int align, int fraction);
int frame_split_command(int argc, char** argv, GString* output);
int frame_change_fraction_command(int argc, char** argv, GString* output);

void frame_apply_layout(HSFrame* frame, herbstluft::Rectangle rect);
void frame_apply_floating_layout(HSFrame* frame, struct HSMonitor* m);
void frame_update_frame_window_visibility(HSFrame* frame);
void reset_frame_colors();
HSFrame* get_toplevel_frame(HSFrame* frame);

void print_frame_tree(HSFrame* frame, GString* output);
void dump_frame_tree(HSFrame* frame, GString* output);
// create apply a described layout to a frame and its subframes
// returns pointer to string that was not parsed yet
// or NULL on an error
char* load_frame_tree(HSFrame* frame, char* description, GString* errormsg);
int find_layout_by_name(char* name);
int find_align_by_name(char* name);

int frame_current_bring(int argc, char** argv, GString* output);
int frame_current_set_selection(int argc, char** argv);
int frame_current_cycle_selection(int argc, char** argv);
int cycle_all_command(int argc, char** argv);
int cycle_frame_command(int argc, char** argv);
void cycle_frame(int direction, int new_window_index, bool skip_invisible);

void frame_unfocus(); // unfocus currently focused window

// get neighbour in a specific direction 'l' 'r' 'u' 'd' (left, right,...)
// returns the neighbour or NULL if there is no one
HSFrame* frame_neighbour(HSFrame* frame, char direction);
int frame_inner_neighbour_index(HSFrame* frame, char direction);
int frame_focus_command(int argc, char** argv, GString* output);

// follow selection to leaf and focus this frame
int frame_focus_recursive(HSFrame* frame);
void frame_do_recursive(HSFrame* frame, void (*action)(HSFrame*), int order);
void frame_do_recursive_data(HSFrame* frame, void (*action)(HSFrame*,void*),
                             int order, void* data);
void frame_hide_recursive(HSFrame* frame);
void frame_show_recursive(HSFrame* frame);
int layout_rotate_command();
// do an action for each client in frame tree
// returns success or failure
int frame_foreach_client(HSFrame* frame, ClientAction action, void* data);

void frame_apply_client_layout_linear(HSFrame* frame,
                                      herbstluft::Rectangle rect, bool vertical);
void frame_apply_client_layout_horizontal(HSFrame* frame,
                                          herbstluft::Rectangle rect);
void frame_apply_client_layout_vertical(HSFrame* frame,
                                        herbstluft::Rectangle rect);
int frame_current_cycle_client_layout(int argc, char** argv, GString* output);
int frame_current_set_client_layout(int argc, char** argv, GString* output);
int frame_split_count_to_root(HSFrame* frame, int align);

// returns the Window that is focused
// returns 0 if there is none
HSClient* frame_focused_client(HSFrame* frame);
bool frame_focus_client(HSFrame* frame, HSClient* client);
bool focus_client(HSClient* client, bool switch_tag, bool switch_monitor);
// moves a window to an other frame
int frame_move_window_command(int argc, char** argv, GString* output);
/// removes the current frame
int frame_remove_command(int argc, char** argv);
int close_or_remove_command(int argc, char** argv);
int close_and_remove_command(int argc, char** argv);
void frame_set_visible(HSFrame* frame, bool visible);
void frame_update_border(Window window, unsigned long color);

int frame_focus_edge(int argc, char** argv, GString* output);
int frame_move_window_edge(int argc, char** argv, GString* output);

bool smart_window_surroundings_active(HSFrame* frame);

#endif

