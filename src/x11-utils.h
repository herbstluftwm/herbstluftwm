#ifndef __HERBST_X11_UTILS_H_
#define __HERBST_X11_UTILS_H_

#include <X11/X.h>

#include "x11-types.h"

// cut a rect out of the window, s.t. the window has geometry rect and a frame
// of width framewidth remains
void window_cut_rect_hole(Window win, int width, int height, int framewidth);
// fill the hole again, i.e. remove all masks
void window_make_intransparent(Window win, int width, int height);

Point2D get_cursor_position();

#endif

