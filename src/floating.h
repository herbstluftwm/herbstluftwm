#ifndef __HERBST_FLOATING_H_
#define __HERBST_FLOATING_H_

#include "types.h"
#include "x11-types.h"

void floating_init();
void floating_destroy();

// utilities
int char_to_direction(char ch);
int find_rectangle_in_direction(RectangleIdxVec& rects, int idx, Direction dir);
int find_rectangle_right_of(RectangleIdxVec rects, int idx);
int find_edge_in_direction(RectangleIdxVec& rects, int idx, Direction dir);
int find_edge_right_of(RectangleIdxVec rects, int idx);

// actual implementations
bool floating_focus_direction(Direction dir);
bool floating_shift_direction(Direction dir);


#endif
