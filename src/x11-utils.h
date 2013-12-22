
#ifndef __HERBST_X11_UTILS_H_
#define __HERBST_X11_UTILS_H_

#include <stddef.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "utils.h"


// cut a rect out of the window, s.t. the window has geometry rect and a frame
// of width framewidth remains
void window_cut_rect_hole(Window win, int width, int height, int framewidth);
void window_make_intransparent(Window win, int width, int height);

#endif

