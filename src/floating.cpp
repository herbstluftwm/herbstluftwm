#include "floating.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "utils.h"
#include "mouse.h"
#include "client.h"
#include "tag.h"
#include "layout.h"
#include "settings.h"

using namespace herbstluft;

static int* g_snap_gap;
static int* g_monitors_locked;

void floating_init() {
    g_snap_gap = &(settings_find("snap_gap")->value.i);
    g_monitors_locked = &(settings_find("monitors_locked")->value.i);
}

void floating_destroy() {
}

enum HSDirection char_to_direction(char ch) {
    switch (ch) {
        case 'u': return DirUp;
        case 'r': return DirRight;
        case 'l': return DirLeft;
        case 'd': return DirDown;
        default:  return (HSDirection)-1;
    }
}

// rectlist_rotate rotates the list of given rectangles, s.t. the direction dir
// becomes the direction "right". idx is some distinguished element, whose
// index may change
static void rectlist_rotate(RectangleIdx* rects, size_t cnt, int* idx,
                                enum HSDirection dir) {
    switch (dir) {
        case DirRight:
            return; // nothing to do
        case DirUp:
            // just flip by the horizontal axis
            FOR (i,0,cnt) {
                auto r = &(rects[i].r);
                r->y = - r->y - r->height;
            }
            // and flip order to reverse the order for rectangles with the same
            // center
            for (int i = 0; i < (cnt - 1 - i); i++) {
                int j = (cnt - 1 - i);
                SWAP(RectangleIdx, rects[i], rects[j]);
            }
            *idx = cnt - 1 - *idx;
            // and then direction up now has become direction down
        case DirDown:
            // flip by the diagonal
            //
            //   *-------------> x     *-------------> x
            //   |   +------+          |   +---+[]
            //   |   |      |     ==>  |   |   |
            //   |   +------+          |   |   |
            //   |   []                |   +---+
            //   V                     V
            FOR (i,0,cnt) {
                auto r = &(rects[i].r);
                SWAP(int, r->x, r->y);
                SWAP(int, r->height, r->width);
            }
            return;
        case DirLeft:
            // flip by the vertical axis
            FOR (i,0,cnt) {
                auto r = &(rects[i].r);
                r->x = - r->x - r->width;
            }
            // and flip order to reverse the order for rectangles with the same
            // center
            for (int i = 0; i < (cnt - 1 - i); i++) {
                int j = (cnt - 1 - i);
                SWAP(RectangleIdx, rects[i], rects[j]);
            }
            *idx = cnt - 1 - *idx;
            return;
    }
}

// returns the found index in the original buffer
int find_rectangle_in_direction(RectangleIdx* rects, size_t cnt, int idx,
                                enum HSDirection dir) {
    rectlist_rotate(rects, cnt, &idx, dir);
    return find_rectangle_right_of(rects, cnt, idx);
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

int find_rectangle_right_of(RectangleIdx* rects, size_t cnt, int idx) {
    auto RC = rects[idx].r;
    int cx = RC.x + RC.width / 2;
    int cy = RC.y + RC.height / 2;
    int write_i = 0; // next rectangle to write
    // filter out rectangles not right of RC
    FOR (i,0,cnt) {
        if (idx == i) continue;
        auto R2 = rects[i].r;
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
        auto R2 = rects[i].r;
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
            idxbest = rects[i].idx;
            ibest = i;
        }
    }
    return idxbest;
}

// returns the found index in the modified buffer
int find_edge_in_direction(RectangleIdx* rects, size_t cnt, int idx,
                                enum HSDirection dir) {
    rectlist_rotate(rects, cnt, &idx, dir);
    int found = find_edge_right_of(rects, cnt, idx);
    if (found < 0) return found;
    // rotate back, by requesting the inverse rotation
    //switch (dir) {
    //    case DirLeft: break; // DirLeft is inverse to itself
    //    case DirRight: break; // DirRight is the identity
    //    case DirUp: dir = DirDown; break; // once was rotated 90 deg counterclockwise..
    //                                      // now has to be rotate 90 deg clockwise back
    //    case DirDown: dir = DirUp; break;
    //}
    rectlist_rotate(rects, cnt, &found, dir);
    rectlist_rotate(rects, cnt, &found, dir);
    rectlist_rotate(rects, cnt, &found, dir);
    return found;
}
int find_edge_right_of(RectangleIdx* rects, size_t cnt, int idx) {
    int xbound = rects[idx].r.x + rects[idx].r.width;
    int ylow = rects[idx].r.y;
    int yhigh = rects[idx].r.y + rects[idx].r.height;
    // only keep rectangles with a x coordinate right of the xbound
    // and with an appropriate y/height
    //
    //      +---------+ - - - - - - - - - - -
    //      |   idx   |   area of intrest
    //      +---------+ - - - - - - - - - - -
    int leftmost = -1;
    int dist = INT_MAX;
    FOR (i,0,cnt) {
        if (i == idx) continue;
        if (rects[i].r.x <= xbound) continue;
        int low = rects[i].r.y;
        int high = low + rects[i].r.height;
        if (!intervals_intersect(ylow, yhigh, low, high)) {
            continue;
        }
        if (rects[i].r.x - xbound < dist) {
            dist = rects[i].r.x - xbound;
            leftmost = i;
        }
    }
    return leftmost;
}


static void collectclients_helper(HSClient* client, void* data) {
    GQueue* q = (GQueue*)data;
    g_queue_push_tail(q, client);
}

bool floating_focus_direction(enum HSDirection dir) {
    if (*g_monitors_locked) { return false; }
    HSTag* tag = get_current_monitor()->tag;
    GQueue* q = g_queue_new();
    tag->frame->foreachClient(collectclients_helper, q);
    int cnt = q->length;
    RectangleIdx* rects = g_new0(RectangleIdx, cnt);
    int i = 0;
    int curfocusidx = -1;
    HSClient* curfocus = get_current_client();
    bool success = true;
    if (curfocus == NULL && cnt == 0) {
        success = false;
    }
    for (GList* cur = q->head; cur != NULL; cur = cur->next, i++) {
        HSClient* client = (HSClient*)cur->data;
        if (curfocus == client) curfocusidx = i;
        rects[i].idx = i;
        rects[i].r = client->dec.last_outer_rect;
    }
    int idx = (cnt > 0)
              ? find_rectangle_in_direction(rects, cnt, curfocusidx, dir)
              : -1;
    if (idx < 0) {
        success = false;
    } else {
        HSClient* client = (HSClient*)g_queue_peek_nth(q, idx);
        client->raise();
        focus_client(client, false, false);
    }
    g_free(rects);
    g_queue_free(q);
    return success;
}

bool floating_shift_direction(enum HSDirection dir) {
    if (*g_monitors_locked) { return false; }
    HSTag* tag = get_current_monitor()->tag;
    HSClient* curfocus = get_current_client();
    if (!curfocus) return false;
    GQueue* q = g_queue_new();
    tag->frame->foreachClient(collectclients_helper, q);
    int cnt = q->length;
    if (cnt == 0) {
        g_queue_free(q);
        return false;
    }
    RectangleIdx* rects = g_new0(RectangleIdx, cnt + 4);
    int i = 0;
    int curfocusidx = -1;
    bool success = true;
    for (GList* cur = q->head; cur != NULL; cur = cur->next, i++) {
        HSClient* client = (HSClient*)cur->data;
        if (curfocus == client) curfocusidx = i;
        rects[i].idx = i;
        rects[i].r = client->dec.last_outer_rect;
    }
    g_queue_free(q);
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
            rects[cnt + i].idx = -1;
            rects[cnt + i].r = tmp[i];
        }
    }
    FOR (i,0, cnt + 4) {
        // expand anything by the snap gap
        rects[i].r.x -= *g_snap_gap;
        rects[i].r.y -= *g_snap_gap;
        rects[i].r.width += 2 * *g_snap_gap;
        rects[i].r.height += 2 * *g_snap_gap;
    }
    // don't apply snapgap to focused client, so there will be exactly
    // *g_snap_gap pixels between the focused client and the found edge
    auto focusrect = curfocus->dec.last_outer_rect;
    int idx = find_edge_in_direction(rects, cnt + 4, curfocusidx, dir);
    if (idx < 0) success = false;
    else {
        // shift client
        int dx = 0, dy = 0;
        auto r = rects[idx].r;
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
        monitor_apply_layout(get_current_monitor());
    }
    g_free(rects);
    return success;
}

