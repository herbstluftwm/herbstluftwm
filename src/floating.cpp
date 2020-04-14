#include "floating.h"

#include <algorithm>
#include <climits>
#include <cstdlib>

#include "client.h"
#include "decoration.h"
#include "layout.h"
#include "monitor.h"
#include "settings.h"
#include "tag.h"
#include "utils.h"

using std::vector;
using std::make_pair;

// rectlist_rotate rotates the list of given rectangles, s.t. the direction dir
// becomes the direction "right". idx is some distinguished element, whose
// index may change
static void rectlist_rotate(RectangleIdxVec& rects, int& idx, Direction dir) {
    // Note: For DirRight, there is nothing to do.

    if (dir == Direction::Up) {
        // just flip by the horizontal axis
        for (auto& r : rects) {
            r.second.y = - r.second.y - r.second.height;
        }
        // and flip order to reverse the order for rectangles with the same
        // center
        reverse(rects.begin(), rects.end());
        idx = rects.size() - 1 - idx;
        // and then direction up now has become direction down
    }

    if (dir == Direction::Up || dir == Direction::Down) {
        // flip by the diagonal
        //
        //   *-------------> x     *-------------> x
        //   |   +------+          |   +---+[]
        //   |   |      |     ==>  |   |   |
        //   |   +------+          |   |   |
        //   |   []                |   +---+
        //   V                     V
        for (auto& r : rects) {
            SWAP(int, r.second.x, r.second.y);
            SWAP(int, r.second.height, r.second.width);
        }
    }

    if (dir == Direction::Left) {
        // flip by the vertical axis
        for (auto& r : rects) {
            r.second.x = - r.second.x - r.second.width;
        }
        // and flip order to reverse the order for rectangles with the same
        // center
        reverse(rects.begin(), rects.end());
        idx = rects.size() - 1 - idx;
    }
}

//! fuzzily find a rectangle in the specified direction
//! returns the found index in the original buffer
int find_rectangle_in_direction(RectangleIdxVec& rects, int idx, Direction dir) {
    rectlist_rotate(rects, idx, dir);
    return find_rectangle_right_of(rects, idx);
}

static bool rectangle_is_right_of(Rectangle RC, Rectangle R2) {
    int cx = RC.x + RC.width / 2;
    int cy = RC.y + RC.height / 2;
    // only consider rectangles right of that with specified idx, called RC. A
    // rectangle R2 is considered right, if the angle of the vector from the
    // center of RC to the center of R2 is in the interval [-45 deg, + 45 deg].
    // In a picture:   ...
    //                /
    //   RC +----------+
    //      |      /   |   area right of RC
    //      |    c     |
    //      |      \   |
    //      +----------+
    //                \...
    int rcx = R2.x + R2.width / 2;
    int rcy = R2.y + R2.height / 2;
    // get vector from center of RC to center of R2
    rcx -= cx;
    rcy -= cy;
    if (rcx < 0) {
        return false;
    }
    if (abs(rcy) > rcx) {
        return false;
    }
    if (rcx == 0 && rcy == 0) {
        // if centers match, then disallow R2 to have a larger width
        return true;
    }
    return true;
}

int find_rectangle_right_of(RectangleIdxVec rects, int idx) {
    auto RC = rects[idx].second;
    int cx = RC.x + RC.width / 2;
    int cy = RC.y + RC.height / 2;
    int write_i = 0; // next rectangle to write
    // filter out rectangles not right of RC
    FOR (i,0,rects.size()) {
        if (idx == i) {
            continue;
        }
        auto R2 = rects[i].second;
        int rcx = R2.x + R2.width / 2;
        int rcy = R2.y + R2.height / 2;
        if (!rectangle_is_right_of(RC, R2)) {
            continue;
        }
        // if two rectangles have exactly the same geometry, then sort by index
        // compare centers and not topleft corner because rectangle_is_right_of
        // does it the same way
        if (rcx == cx && rcy == cy) {
            if (i < idx) {
                continue;
            }
        }
        if (i == write_i) { write_i++; }
        else {
            rects[write_i++] = rects[i];
        }
    }
    // find the rectangle with the smallest distance to RC
    if (write_i == 0) {
        return -1;
    }
    int idxbest = -1;
    int ibest = -1;
    int distbest = INT_MAX;
    FOR (i,0,write_i) {
        auto R2 = rects[i].second;
        int rcx = R2.x + R2.width / 2;
        int rcy = R2.y + R2.height / 2;
                            // another method that checks the closes point
        int anchor_y = rcy; // (rcy > cy) ? rcy : std::min(rcy + R2.height, cy);
        int anchor_x = rcx; // std::max(cx, R2.x);
        // get manhatten distance to the anchor
        int dist = abs(anchor_x - cx) + abs(anchor_y - cy);
        if (dist < distbest
            || (dist == distbest && ibest > i)) {
            distbest = dist;
            idxbest = rects[i].first;
            ibest = i;
        }
    }
    return idxbest;
}

// returns the found index in the modified buffer
int find_edge_in_direction(RectangleIdxVec& rects, int idx, Direction dir)
{
    rectlist_rotate(rects, idx, dir);
    int found = find_edge_right_of(rects, idx);
    if (found < 0) {
        return found;
    }
    // rotate back, by requesting the inverse rotation
    //switch (dir) {
    //    case DirLeft: break; // DirLeft is inverse to itself
    //    case DirRight: break; // DirRight is the identity
    //    case DirUp: dir = DirDown; break; // once was rotated 90 deg counterclockwise..
    //                                      // now has to be rotate 90 deg clockwise back
    //    case DirDown: dir = DirUp; break;
    //}
    rectlist_rotate(rects, found, dir);
    rectlist_rotate(rects, found, dir);
    rectlist_rotate(rects, found, dir);
    return found;
}
int find_edge_right_of(RectangleIdxVec rects, int idx) {
    int xbound = rects[idx].second.x + rects[idx].second.width;
    int ylow = rects[idx].second.y;
    int yhigh = rects[idx].second.y + rects[idx].second.height;
    // only keep rectangles with a x coordinate right of the xbound
    // and with an appropriate y/height
    //
    //      +---------+ - - - - - - - - - - -
    //      |   idx   |   area of intrest
    //      +---------+ - - - - - - - - - - -
    int leftmost = -1;
    int dist = INT_MAX;
    FOR (i,0,rects.size()) {
        if (i == idx) {
            continue;
        }
        if (rects[i].second.x <= xbound) {
            continue;
        }
        int low = rects[i].second.y;
        int high = low + rects[i].second.height;
        if (!intervals_intersect(ylow, yhigh, low, high)) {
            continue;
        }
        if (rects[i].second.x - xbound < dist) {
            dist = rects[i].second.x - xbound;
            leftmost = i;
        }
    }
    return leftmost;
}


bool floating_focus_direction(Direction dir) {
    if (g_settings->monitors_locked()) { return false; }
    HSTag* tag = get_current_monitor()->tag;
    vector<Client*> clients;
    RectangleIdxVec rects;
    int idx = 0;
    int curfocusidx = -1;
    Client* curfocus = get_current_client();
    tag->foreachClient([&](Client* c) {
        clients.push_back(c);
        rects.push_back(make_pair(idx,c->dec->last_outer()));
        if (c == curfocus) {
            curfocusidx = idx;
        }
        idx++;
    });
    if (curfocusidx < 0 || idx <= 0) {
        return false;
    }
    idx = find_rectangle_in_direction(rects, curfocusidx, dir);
    if (idx < 0) {
        return false;
    }
    focus_client(clients[idx], false, false, true);
    return true;
}

//! when moving the given client on tag in the specified direction
//! report the vector to travel until the collision happens
Point2D find_rectangle_collision_on_tag(HSTag* tag, Client* curfocus, Direction dir) {
    vector<Client*> clients;
    RectangleIdxVec rects;
    int idx = 0;
    int curfocusidx = -1;
    tag->foreachClient([&](Client* c) {
        clients.push_back(c);
        rects.push_back(make_pair(idx,c->dec->last_outer()));
        if (c == curfocus) {
            curfocusidx = idx;
        }
        idx++;
    });
    if (curfocusidx < 0 || idx <= 0) {
        return {0, 0};
    }
    // add artifical rects for screen edges
    {
        auto mr = get_current_monitor()->getFloatingArea();
        vector<Rectangle> tmp = {
            { mr.x, mr.y,               mr.width, 0 }, // top
            { mr.x, mr.y,               0, mr.height }, // left
            { mr.x + mr.width, mr.y,    0, mr.height }, // right
            { mr.x, mr.y + mr.height,   mr.y + mr.width, 0 }, // bottom
        };
        for (const auto& r : tmp) {
            rects.push_back(make_pair(-1, r));
        }
    }
    for (auto& r : rects) {
        // expand anything by the snap gap
        r.second.x -= g_settings->snap_gap();
        r.second.y -= g_settings->snap_gap();
        r.second.width += 2 * g_settings->snap_gap();
        r.second.height += 2 * g_settings->snap_gap();
    }
    // don't apply snapgap to focused client, so there will be exactly
    // snap_gap pixels between the focused client and the found edge
    auto focusrect = curfocus->dec->last_outer();
    idx = find_edge_in_direction(rects, curfocusidx, dir);
    if (idx < 0) {
        return {0, 0};
    }
    // shift client
    int dx = 0, dy = 0;
    auto r = rects[idx].second;
    //printf("edge: %dx%d at %d,%d\n", r.width, r.height, r.x, r.y);
    //printf("focus: %dx%d at %d,%d\n", focusrect.width, focusrect.height, focusrect.x, focusrect.y);
    switch (dir) {
        //          delta = new edge  -  old edge
        case Direction::Right: dx = r.x  -   (focusrect.x + focusrect.width); break;
        case Direction::Left:  dx = r.x + r.width   -   focusrect.x; break;
        case Direction::Down:  dy = r.y  -  (focusrect.y + focusrect.height); break;
        case Direction::Up:    dy = r.y + r.height  -  focusrect.y; break;
    }
    return {dx, dy};
}

bool floating_shift_direction(Direction dir) {
    if (g_settings->monitors_locked()) { return false; }
    HSTag* tag = get_current_monitor()->tag;
    Client* curfocus = tag->focusedClient();
    if (!curfocus || !curfocus->is_client_floated()) {
        return false;
    }
    Point2D delta = find_rectangle_collision_on_tag(tag, curfocus, dir);
    if (delta == Point2D{0, 0}) {
        return false;
    }
    curfocus->float_size_.x += delta.x;
    curfocus->float_size_.y += delta.y;
    get_current_monitor()->applyLayout();
    return true;
}


static bool resize_by_delta(Client* client, Direction dir, int delta) {
    int new_width = client->float_size_.width;
    int new_height = client->float_size_.height;
    if (dir == Direction::Right || dir == Direction::Left) {
        new_width += delta;
    }
    if (dir == Direction::Up || dir == Direction::Down) {
        new_height += delta;
    }
    client->applysizehints(&new_width, &new_height);
    if (new_width == client->float_size_.width
            && new_height == client->float_size_.height)
    {
        return false;
    }
    // growing to the left or down
    if (dir == Direction::Right || dir == Direction::Down) {
        client->float_size_.width = new_width;
        client->float_size_.height = new_height;
    } else if (dir == Direction::Left || dir == Direction::Up) {
        client->float_size_.x -= new_width - client->float_size_.width;
        client->float_size_.y -= new_height - client->float_size_.height;
        client->float_size_.width = new_width;
        client->float_size_.height = new_height;
    }
    return true;
}

static bool grow_into_direction(HSTag* tag, Client* client, Direction dir) {
    Point2D delta = find_rectangle_collision_on_tag(tag, client, dir);
    if (delta == Point2D{0, 0}) {
        return false;
    }
    if (!resize_by_delta(client, dir, std::abs(delta.x) + std::abs(delta.y))) {
        // TODO: if the delta was too small and made 0 by applysizehints()
        // then we need to do something smarter in the future
        return false;
    }
    return true;
}

static bool shrink_into_direction(Client* client, Direction dir) {
    int delta_width = client->float_size_.width / -2;
    int delta_height = client->float_size_.height / -2;
    switch (dir) {
        case Direction::Right:
            // don't let windows shrink arbitrarily. 100 px is hopefully ok
            // this here is bigger than WINDOW_MIN_WIDTH/_HEIGHT on purpose
            if (client->float_size_.width < 100) {
                return false;
            }
            return resize_by_delta(client, Direction::Left, delta_width);
        case Direction::Left:
            if (client->float_size_.width < 100) {
                return false;
            }
            return resize_by_delta(client, Direction::Right, delta_width);
        case Direction::Down:
            if (client->float_size_.height < 100) {
                return false;
            }
            return resize_by_delta(client, Direction::Up, delta_height);
        case Direction::Up:
            if (client->float_size_.height < 100) {
                return false;
            }
            return resize_by_delta(client, Direction::Down, delta_height);
    }
    return false;
}

bool floating_resize_direction(HSTag* tag, Client* client, Direction dir)
{
    if (g_settings->monitors_locked()) {
        return false;
    }
    if (!client || !client->is_client_floated()) {
        return false;
    }
    // 1. Try to grow into a specific direction
    if (grow_into_direction(tag, client, dir)) {
        return true;
    }
    // 2. try to shrink into a specific direction
    return shrink_into_direction(client, dir);
}
