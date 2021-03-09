#ifndef __HERBST_X11_UTILS_H_
#define __HERBST_X11_UTILS_H_

#include <X11/X.h>
#include <vector>

#include "rectangle.h"
#include "x11-types.h"

// cut the specified rectangles out of the window
void window_cut_rect_holes(XConnection& X,
                           Window win, int width, int height,
                           const std::vector<Rectangle>& holes);
// fill the hole again, i.e. remove all masks
void window_make_intransparent(XConnection& X,
                               Window win, int width, int height);

Point2D get_cursor_position();

#endif

