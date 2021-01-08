#ifndef __HERBST_FLOATING_H_
#define __HERBST_FLOATING_H_

#include "types.h"
#include "rectangle.h"

class Client;
class HSTag;

/**
 * @brief The Floating class collects algorithms related to
 * the geometry of floating windows
 */
class Floating {
public:
    // utilities
    static int find_rectangle_in_direction(RectangleIdxVec& rects, int idx, Direction dir);
    static int find_rectangle_right_of(RectangleIdxVec rects, int idx);
    static int find_edge_in_direction(RectangleIdxVec& rects, int idx, Direction dir);
    static int find_edge_right_of(RectangleIdxVec rects, int idx);

    // actual implementations
    static bool focusDirection(Direction dir);
    static bool shiftDirection(Direction dir);
    static bool resizeDirection(HSTag* tag, Client* client, Direction dir);

    static Point2D smartPlacement(HSTag* tag, Client* client, Point2D area, int gap);

private:
    static Point2D find_rectangle_collision_on_tag(HSTag* tag, Client* curfocus, Direction dir, Rectangle curfocusrect = {0,0,-1,-1});
    static bool grow_into_direction(HSTag* tag, Client* client, Direction dir);
    static bool shrink_into_direction(Client* client, Direction dir);
};

#endif
