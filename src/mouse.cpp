#include "mouse.h"

#include <cstdlib>
#include <sstream>

#include "client.h"
#include "completion.h"
#include "decoration.h"
#include "monitor.h"
#include "settings.h"
#include "tag.h"
#include "utils.h"

using std::pair;
using std::string;
using std::stringstream;
using std::vector;

MouseCombo::MouseCombo(unsigned int modifiers, unsigned int button)
    : button_(button)
{
    modifiers_ = modifiers;
}

vector<pair<string, unsigned int>> MouseCombo::name2button =
{
    { "Button1",  Button1 },
    { "Button2",  Button2 },
    { "Button3",  Button3 },
    { "Button4",  Button4 },
    { "Button5",  Button5 },
    { "B1",       Button1 },
    { "B2",       Button2 },
    { "B3",       Button3 },
    { "B4",       Button4 },
    { "B5",       Button5 },
};

template<>
MouseCombo Converter<MouseCombo>::parse(const string& source)
{
    auto mws = Converter<ModifiersWithString>::parse(source);
    for (const auto& p : MouseCombo::name2button) {
        if (mws.suffix_ == p.first) {
            return MouseCombo(mws.modifiers_, p.second);
        }
    }
    stringstream msg;
    msg << "Unknown mouse button \"" << mws.suffix_ << "\"";
    throw std::invalid_argument(msg.str());
}

template<>
string Converter<MouseCombo>::str(MouseCombo payload)
{
    ModifiersWithString mws(payload.modifiers_, "?");
    for (const auto& p : MouseCombo::name2button) {
        if (payload.button_ == p.second) {
            mws.suffix_ = p.first;
            break;
        }
    }
    return Converter<ModifiersWithString>::str(mws);
}

template<>
void Converter<MouseCombo>::complete(Completion& outerComplete, MouseCombo const* relativeTo)
{
    auto buttonCompleter = [](Completion& complete, string prefix) {
        for (const auto& p : MouseCombo::name2button) {
            complete.full(prefix + p.first);
        }
    };
    ModifiersWithString::complete(outerComplete, buttonCompleter);
}

struct SnapData {
    Client*       client = nullptr;
    Rectangle      rect;
    enum SnapFlags  flags = {};
    int             dx = 0, dy = 0; // the vector from client to other to make them snap
};

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
    struct SnapData d = {};
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
    d.rect.x += monitor->rect->x + monitor->pad_left;
    d.rect.y += monitor->rect->y + monitor->pad_up;
    d.flags     = flags;
    d.dx        = distance;
    d.dy        = distance;

    // snap to monitor edges
    Monitor* m = monitor;
    if (flags & SNAP_EDGE_TOP) {
        snap_1d(d.rect.y, m->rect->y + m->pad_up + g_settings->snap_gap(), &d.dy);
    }
    if (flags & SNAP_EDGE_LEFT) {
        snap_1d(d.rect.x, m->rect->x + m->pad_left + g_settings->snap_gap(), &d.dx);
    }
    if (flags & SNAP_EDGE_RIGHT) {
        snap_1d(d.rect.x + d.rect.width, m->rect->x + m->rect->width - m->pad_right - g_settings->snap_gap(), &d.dx);
    }
    if (flags & SNAP_EDGE_BOTTOM) {
        snap_1d(d.rect.y + d.rect.height, m->rect->y + m->rect->height - m->pad_down - g_settings->snap_gap(), &d.dy);
    }

    // snap to other clients
    tag->foreachClient([&d] (Client* c) { client_snap_helper(c, &d); });

    // write back results
    if (abs(d.dx) < abs(distance)) {
        *return_dx = d.dx;
    }
    if (abs(d.dy) < abs(distance)) {
        *return_dy = d.dy;
    }
}

