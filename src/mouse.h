

#ifndef __HERBSTLUFT_MOUSE_H_
#define __HERBSTLUFT_MOUSE_H_

#include <X11/Xlib.h>

void mouse_init();
void mouse_destroy();


void mouse_grab(Window win);
void mouse_start_drag(XEvent* ev);
void mouse_stop_drag(XEvent* ev);
void handle_motion_event(XEvent* ev);

#endif

