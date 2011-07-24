
#ifndef __HERBSTLUFT_LAYOUT_H_
#define __HERBSTLUFT_LAYOUT_H_

#include <glib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <X11/Xlib.h>

enum {
    LAYOUT_VERTICAL = 0,
    LAYOUT_HORIZONTAL,
};

enum {
    TYPE_CLIENTS = 0,
    TYPE_FRAMES,
};

#define FRACTION_UNIT 10000

struct HSFrame;
struct HSTag;

typedef struct HSLayout {
    int align;         // LAYOUT_VERTICAL or LAYOUT_HORIZONTAL
    struct HSFrame* a; // first child
    struct HSFrame* b; // second child
    int selection;
    int fraction; // size of first child relative to whole size
                  // FRACTION_UNIT means full size
                  // FRACTION_UNIT/2 means 50%
} HSLayout;

typedef struct HSFrame {
    union {
        HSLayout* layout;
        struct {
            Window* buf;
            size_t  count;
            int     selection;
        } clients;
    } content;
    int type;
    struct HSFrame* parent;
} HSFrame;


typedef struct HSMonitor {
    struct HSTag*      tag;    // currently viewed tag
    XRectangle  rect;   // area for this monitor
} HSMonitor;

typedef struct HSTag {
    GString*    name;   // name of this tag
    HSFrame*    frame;  // the master frame
} HSTag;

// globals
GArray*     g_tags;
GArray*     g_monitors;
int         g_cur_monitor;

// functions
void layout_init();
void layout_destroy();
// for frames
HSFrame* frame_create_empty();
void frame_insert_window(HSFrame* frame, Window window);
// removes window from a frame/subframes
// returns true, if window was found. else: false
bool frame_remove_window(HSFrame* frame, Window window);
// destroys a frame and all its childs
// then all Windows in it are collected and returned
// YOU have to g_free the resulting window-buf
void frame_destroy(HSFrame* frame, Window** buf, size_t* count);

void frame_apply_layout(HSFrame* frame, XRectangle rect);

void print_frame_tree(HSFrame* frame, int indent, GString** output);


// for tags
HSTag* add_tag(char* name);
// for monitors
// ads a new monitor to g_monitors and returns a pointer to it
HSMonitor* find_monitor_with_tag(HSTag* tag);
HSMonitor* add_monitor(XRectangle rect);
void ensure_monitors_are_available();

#endif


