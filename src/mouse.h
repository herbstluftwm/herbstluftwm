/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_MOUSE_H_
#define __HERBSTLUFT_MOUSE_H_

#include <X11/Xlib.h>
#include "glib-backports.h"
#include "x11-types.h"
#include "types.h"

// various snap-flags
enum SnapFlags {
    // which edges are considered to snap
    SNAP_EDGE_TOP       = 0x01,
    SNAP_EDGE_BOTTOM    = 0x02,
    SNAP_EDGE_LEFT      = 0x04,
    SNAP_EDGE_RIGHT     = 0x08,
    SNAP_EDGE_ALL       =
        SNAP_EDGE_TOP | SNAP_EDGE_BOTTOM | SNAP_EDGE_LEFT | SNAP_EDGE_RIGHT,
};

// forward declarations
class HSClient;
class HSMonitor;
class HSTag;

void mouse_init();
void mouse_destroy();


typedef void (*MouseDragFunction)(XMotionEvent*);
typedef void (*MouseFunction)(HSClient* client, int argc, char** argv);

typedef struct MouseBinding {
    unsigned int modifiers;
    unsigned int button;
    MouseFunction action;
    int     argc; // additional arguments
    char**  argv;
} MouseBinding;

int mouse_binding_equals(MouseBinding* a, MouseBinding* b);

int mouse_bind_command(int argc, char** argv, Output output);
int mouse_unbind_all();
MouseBinding* mouse_binding_find(unsigned int modifiers, unsigned int button);

unsigned int string2button(const char* name);
MouseFunction string2mousefunction(char* name);

void grab_client_buttons(HSClient* client, bool focused);

void mouse_handle_event(XEvent* ev);
void mouse_initiate_drag(HSClient* client, MouseDragFunction function);
void mouse_stop_drag();
bool mouse_is_dragging();
void handle_motion_event(XEvent* ev);

// get the vector to snap a client to it's neighbour
void client_snap_vector(HSClient* client, HSMonitor* monitor,
                        enum SnapFlags flags,
                        int* return_dx, int* return_dy);

bool is_point_between(int point, int left, int right);
// tells if the intervals [a_left, a_right) [b_left, b_right) intersect
bool intervals_intersect(int a_left, int a_right, int b_left, int b_right);

void mouse_initiate_move(HSClient* client, int argc, char** argv);
void mouse_initiate_zoom(HSClient* client, int argc, char** argv);
void mouse_initiate_resize(HSClient* client, int argc, char** argv);
void mouse_call_command(HSClient* client, int argc, char** argv);
/* some mouse drag functions */
void mouse_function_move(XMotionEvent* me);
void mouse_function_resize(XMotionEvent* me);
void mouse_function_zoom(XMotionEvent* me);

void complete_against_mouse_buttons(const char* needle, char* prefix, Output output);

#endif

