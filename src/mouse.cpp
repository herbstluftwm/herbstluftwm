#include "mouse.h"

#include <X11/X.h>
#include <cstdlib>

#include "client.h"
#include "decoration.h"
#include "frametree.h"
#include "keymanager.h"
#include "layout.h"
#include "monitor.h"
#include "root.h"
#include "settings.h"
#include "tag.h"
#include "utils.h"

#define CLEANMASK(mask)         ((mask) & ~(numlockMask|LockMask))
#define REMOVEBUTTONMASK(mask) ((mask) & \
    ~( Button1Mask \
     | Button2Mask \
     | Button3Mask \
     | Button4Mask \
     | Button5Mask ))

using std::string;
using std::vector;

struct SnapData {
    Client*       client;
    Rectangle      rect;
    enum SnapFlags  flags;
    int             dx, dy; // the vector from client to other to make them snap
};

int mouse_binding_equals(const MouseBinding* a, const MouseBinding* b) {
    unsigned int numlockMask = Root::get()->keys()->getNumlockMask();
    if((REMOVEBUTTONMASK(CLEANMASK(a->modifiers))
        == REMOVEBUTTONMASK(CLEANMASK(b->modifiers)))
        && (a->button == b->button)) {
        return 0;
    } else {
        return -1;
    }
}

bool is_point_between(int point, int left, int right) {
    return (point < right && point >= left);
}

// compute vector to snap a point to an edge
static void snap_1d(int x, int edge, int* delta) {
    // whats the vector from subject to edge?
    int cur_delta = edge - x;
    // if distance is smaller then all other deltas
    if (abs(cur_delta) < abs(*delta)) {
        // then snap it, i.e. save vector
        *delta = cur_delta;
    }
}

static void client_snap_helper(Client* candidate, struct SnapData* d) {
    if (candidate == d->client) {
        return;
    }
    auto subject  = d->rect;
    auto other    = candidate->dec->last_outer();
    // increase other by snap gap
    other.x -= g_settings->snap_gap();
    other.y -= g_settings->snap_gap();
    other.width += g_settings->snap_gap() * 2;
    other.height += g_settings->snap_gap() * 2;
    if (intervals_intersect(other.y, other.y + other.height, subject.y, subject.y + subject.height)) {
        // check if x can snap to the right
        if (d->flags & SNAP_EDGE_RIGHT) {
            snap_1d(subject.x + subject.width, other.x, &d->dx);
        }
        // or to the left
        if (d->flags & SNAP_EDGE_LEFT) {
            snap_1d(subject.x, other.x + other.width, &d->dx);
        }
    }
    if (intervals_intersect(other.x, other.x + other.width, subject.x, subject.x + subject.width)) {
        // if we can snap to the top
        if (d->flags & SNAP_EDGE_TOP) {
            snap_1d(subject.y, other.y + other.height, &d->dy);
        }
        // or to the bottom
        if (d->flags & SNAP_EDGE_BOTTOM) {
            snap_1d(subject.y + subject.height, other.y, &d->dy);
        }
    }
    return;
}

// get the vector to snap a client to it's neighbour
void client_snap_vector(Client* client, Monitor* monitor,
                        enum SnapFlags flags,
                        int* return_dx, int* return_dy) {
    struct SnapData d;
    HSTag* tag = monitor->tag;
    int distance = std::max(0, g_settings->snap_distance());
    // init delta
    *return_dx = 0;
    *return_dy = 0;
    if (!distance) {
        // nothing to do
        return;
    }
    d.client    = client;
    // translate client rectangle to global coordinates
    d.rect      = client->outer_floating_rect();
    d.rect.x += monitor->rect.x + monitor->pad_left;
    d.rect.y += monitor->rect.y + monitor->pad_up;
    d.flags     = flags;
    d.dx        = distance;
    d.dy        = distance;

    // snap to monitor edges
    Monitor* m = monitor;
    if (flags & SNAP_EDGE_TOP) {
        snap_1d(d.rect.y, m->rect.y + m->pad_up + g_settings->snap_gap(), &d.dy);
    }
    if (flags & SNAP_EDGE_LEFT) {
        snap_1d(d.rect.x, m->rect.x + m->pad_left + g_settings->snap_gap(), &d.dx);
    }
    if (flags & SNAP_EDGE_RIGHT) {
        snap_1d(d.rect.x + d.rect.width, m->rect.x + m->rect.width - m->pad_right - g_settings->snap_gap(), &d.dx);
    }
    if (flags & SNAP_EDGE_BOTTOM) {
        snap_1d(d.rect.y + d.rect.height, m->rect.y + m->rect.height - m->pad_down - g_settings->snap_gap(), &d.dy);
    }

    // snap to other clients
    tag->frame->root_->foreachClient([&d] (Client* c) { client_snap_helper(c, &d); });

    // write back results
    if (abs(d.dx) < abs(distance)) {
        *return_dx = d.dx;
    }
    if (abs(d.dy) < abs(distance)) {
        *return_dy = d.dy;
    }
}

