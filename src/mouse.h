

#ifndef __HERBSTLUFT_MOUSE_H_
#define __HERBSTLUFT_MOUSE_H_

#include <X11/Xlib.h>

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

// foreward declarations
struct HSClient;
struct HSTag;

void mouse_init();
void mouse_destroy();

typedef void (*MouseFunction)(XMotionEvent*);

typedef struct MouseBinding {
    unsigned int modifiers;
    unsigned int button;
    MouseFunction function;
} MouseBinding;

int mouse_binding_equals(MouseBinding* a, MouseBinding* b);

void mouse_bind_function(unsigned int modifiers, unsigned int button,
                         MouseFunction function);
int mouse_bind_command(int argc, char** argv);
int mouse_unbind_all();
MouseBinding* mouse_binding_find(unsigned int modifiers, unsigned int button);

unsigned int string2button(char* name);
MouseFunction string2mousefunction(char* name);

void mouse_start_drag(XEvent* ev);
void mouse_stop_drag();
void handle_motion_event(XEvent* ev);

// get the vector to snap a client to it's neighbour
void client_snap_vector(struct HSClient* client, struct HSTag* tag,
                        enum SnapFlags flags,
                        int* return_dx, int* return_dy);

/* some mouse functions */
void mouse_function_move(XMotionEvent* me);
void mouse_function_resize(XMotionEvent* me);
void mouse_function_zoom(XMotionEvent* me);


#endif

