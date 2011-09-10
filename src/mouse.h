

#ifndef __HERBSTLUFT_MOUSE_H_
#define __HERBSTLUFT_MOUSE_H_

#include <X11/Xlib.h>

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
void mouse_regrab_all();

unsigned int string2button(char* name);
MouseFunction string2mousefunction(char* name);

void mouse_start_drag(XEvent* ev);
void mouse_stop_drag(XEvent* ev);
void handle_motion_event(XEvent* ev);

/* some mouse functions */
void mouse_function_move(XMotionEvent* me);
void mouse_function_resize(XMotionEvent* me);

#endif

