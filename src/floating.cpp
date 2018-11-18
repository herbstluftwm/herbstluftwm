#include "floating.h"

#include <cstdlib>
#include <cstdio>
#include <algorithm>

#include "utils.h"
#include "mouse.h"
#include "client.h"
#include "tag.h"
#include "layout.h"
#include "settings.h"
#include "utils.h"

using namespace std;

void floating_init() {
}

void floating_destroy() {
}

int char_to_direction(char ch) {
    switch (ch) {
        case 'u': return DirUp;
        case 'r': return DirRight;
        case 'l': return DirLeft;
        case 'd': return DirDown;
        default:  return -1;
    }
}

// rectlist_rotate rotates the list of given rectangles, s.t. the direction dir
// becomes the direction "right". idx is some distinguished element, whose
// index may change
static void rectlist_rotate(RectangleIdxVec& rects, int& idx,
                                enum HSDirection dir) {
    // Note: For DirRight, there is nothing to do.

    if (dir == DirUp) {
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

    if (dir == DirUp || dir == DirDown) {
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

    if (dir == DirLeft) {
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

// returns the found index in the original buffer
int find_rectangle_in_direction(RectangleIdxVec& rects, int idx,
                                enum HSDirection dir) {
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
    if (rcx < 0) return false;
    if (abs(rcy) > rcx) return false;
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
        if (idx == i) continue;
        auto R2 = rects[i].second;
        int rcx = R2.x + R2.width / 2;
        int rcy = R2.y + R2.height / 2;
        if (!rectangle_is_right_of(RC, R2)) continue;
        // if two rectangles have exactly the same geometry, then sort by index
        // compare centers and not topleft corner because rectangle_is_right_of
        // does it the same way
        if (rcx == cx && rcy == cy) {
            if (i < idx) continue;
        }
        if (i == write_i) { write_i++; }
        else {
            rects[write_i++] = rects[i];
        }
    }
    // find the rectangle with the smallest distance to RC
    if (write_i == 0) return -1;
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
int find_edge_in_direction(RectangleIdxVec& rects, int idx, enum HSDirection dir)
{
    rectlist_rotate(rects, idx, dir);
    int found = find_edge_right_of(rects, idx);
    if (found < 0) return found;
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
        if (i == idx) continue;
        if (rects[i].second.x <= xbound) continue;
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


bool floating_focus_direction(enum HSDirection dir) {
    if (g_settings->monitors_locked()) { return false; }
    HSTag* tag = get_current_monitor()->tag;
    vector<HSClient*> clients;
    RectangleIdxVec rects;
    int idx = 0;
    int curfocusidx = -1;
    HSClient* curfocus = get_current_client();
    tag->frame->foreachClient([&](HSClient* c) {
        clients.push_back(c);
        rects.push_back(make_pair(idx,c->dec.last_outer()));
        if (c == curfocus) curfocusidx = idx;
        idx++;
    });
    if (curfocusidx < 0 || idx <= 0) {
        return false;
    }
    idx = find_rectangle_in_direction(rects, curfocusidx, dir);
    if (idx < 0) {
        return false;
    }
    clients[idx]->raise();
    focus_client(clients[idx], false, false);
    return true;
}

bool floating_shift_direction(enum HSDirection dir) {
    if (g_settings->monitors_locked()) { return false; }
    HSTag* tag = get_current_monitor()->tag;
    vector<HSClient*> clients;
    RectangleIdxVec rects;
    int idx = 0;
    int curfocusidx = -1;
    HSClient* curfocus = get_current_client();
    tag->frame->foreachClient([&](HSClient* c) {
        clients.push_back(c);
        rects.push_back(make_pair(idx,c->dec.last_outer()));
        if (c == curfocus) curfocusidx = idx;
        idx++;
    });
    if (curfocusidx < 0 || idx <= 0) {
        return false;
    }
    // add artifical rects for screen edges
    {
        auto mr = get_current_monitor()->getFloatingArea();
        Rectangle tmp[4] = {
            { mr.x, mr.y,               mr.width, 0 }, // top
            { mr.x, mr.y,               0, mr.height }, // left
            { mr.x + mr.width, mr.y,    0, mr.height }, // right
            { mr.x, mr.y + mr.height,   mr.y + mr.width, 0 }, // bottom
        };
        FOR (i,0,4) {
            rects.push_back(make_pair(-1, tmp[i]));
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
    auto focusrect = curfocus->dec.last_outer();
    idx = find_edge_in_direction(rects, curfocusidx, dir);
    if (idx < 0) return false;
    // shift client
    int dx = 0, dy = 0;
    auto r = rects[idx].second;
    //printf("edge: %dx%d at %d,%d\n", r.width, r.height, r.x, r.y);
    //printf("focus: %dx%d at %d,%d\n", focusrect.width, focusrect.height, focusrect.x, focusrect.y);
    switch (dir) {
        //          delta = new edge  -  old edge
        case DirRight: dx = r.x  -   (focusrect.x + focusrect.width); break;
        case DirLeft:  dx = r.x + r.width   -   focusrect.x; break;
        case DirDown:  dy = r.y  -  (focusrect.y + focusrect.height); break;
        case DirUp:    dy = r.y + r.height  -  focusrect.y; break;
    }
    //printf("dx=%d, dy=%d\n", dx, dy);
    curfocus->float_size_.x += dx;
    curfocus->float_size_.y += dy;
    get_current_monitor()->applyLayout();
    return true;
}

