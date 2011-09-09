

#ifndef __HERBSTLUFT_MOUSE_H_
#define __HERBSTLUFT_MOUSE_H_

#include <X11/Xlib.h>

void mouse_init();
void mouse_destroy();

typedef void (*MouseFunction)(XEvent*);

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

unsigned int string2button(char* name);
MouseFunction string2mousefunction(char* name);

void mouse_start_drag(XEvent* ev);
void mouse_stop_drag(XEvent* ev);
void handle_motion_event(XEvent* ev);

#endif

