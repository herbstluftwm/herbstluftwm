#ifndef __HERBSTLUFT_MOUSE_H_
#define __HERBSTLUFT_MOUSE_H_

#include <X11/Xlib.h>

#include "glib-backports.h"
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
class Client;
class Monitor;

typedef void (*MouseDragFunction)(XMotionEvent*);
typedef void (*MouseFunction)(Client* client, const std::vector<std::string> &cmd);

class MouseBinding {
public:
    unsigned int modifiers;
    unsigned int button;
    MouseFunction action;
    std::vector<std::string> cmd;
};

int mouse_binding_equals(const MouseBinding* a, const MouseBinding* b);

int mouse_bind_command(int argc, char** argv, Output output);
int mouse_unbind_all();
MouseBinding* mouse_binding_find(unsigned int modifiers, unsigned int button);

unsigned int string2button(const char* name);
MouseFunction string2mousefunction(char* name);

void grab_client_buttons(Client* client, bool focused);

void mouse_handle_event(XEvent* ev);
void mouse_initiate_drag(Client* client, MouseDragFunction function);
void mouse_stop_drag();
bool mouse_is_dragging();
void handle_motion_event(XEvent* ev);

// get the vector to snap a client to it's neighbour
void client_snap_vector(Client* client, Monitor* monitor,
                        enum SnapFlags flags,
                        int* return_dx, int* return_dy);

bool is_point_between(int point, int left, int right);

void mouse_initiate_move(Client* client, const std::vector<std::string> &cmd);
void mouse_initiate_zoom(Client* client, const std::vector<std::string> &cmd);
void mouse_initiate_resize(Client* client, const std::vector<std::string> &cmd);
void mouse_call_command(Client* client, const std::vector<std::string> &cmd);
/* some mouse drag functions */
void mouse_function_move(XMotionEvent* me);
void mouse_function_resize(XMotionEvent* me);
void mouse_function_zoom(XMotionEvent* me);

void complete_against_mouse_buttons(const char* needle, char* prefix, Output output);

#endif

