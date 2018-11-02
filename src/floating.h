/** Copyright 2011-2014 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBST_FLOATING_H_
#define __HERBST_FLOATING_H_

#include <sys/types.h>
#include "x11-types.h"
#include <stdbool.h>

enum HSDirection {
    DirRight,
    DirLeft,
    DirUp,
    DirDown,
};

void floating_init();
void floating_destroy();

typedef std::vector<std::pair<int,Rectangle>> RectangleIdxVec;

// utilities
int char_to_direction(char ch);
int find_rectangle_in_direction(RectangleIdxVec& rects, int idx, enum HSDirection dir);
int find_rectangle_right_of(RectangleIdxVec rects, int idx);
int find_edge_in_direction(RectangleIdxVec& rects, int idx,
                                enum HSDirection dir);
int find_edge_right_of(RectangleIdxVec rects, int idx);

// actual implementations
bool floating_focus_direction(enum HSDirection dir);
bool floating_shift_direction(enum HSDirection dir);


#endif
