#ifndef __HERBST_FLOATING_H_
#define __HERBST_FLOATING_H_

#include <sys/types.h>
#include "x11-types.h"

struct Direction {
    enum V {
        Up,
        Right,
        Down,
        Left,
    };

    Direction(V v) : value(v), valid(true) {}
    Direction(char ch);

    V value;
    bool valid = false;
};
bool operator==(const Direction& lhs, const Direction::V& rhs);

void floating_init();
void floating_destroy();

typedef std::vector<std::pair<int,Rectangle>> RectangleIdxVec;

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
