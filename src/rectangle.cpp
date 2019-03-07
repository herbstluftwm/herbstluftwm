#include "rectangle.h"

#include <algorithm>
#include <ostream>
#include <vector>

#include "glib-backports.h"
#include "ipc-protocol.h"
#include "utils.h"

using std::endl;

static bool rects_intersect(const Rectangle &a, const Rectangle &b) {
    bool is = true;
    is = is && intervals_intersect(a.x, a.x + a.width,
                                   b.x, b.x + b.width);
    is = is && intervals_intersect(a.y, a.y + a.height,
                                   b.y, b.y + b.height);
    return is;
}

static Rectangle intersection_area(const Rectangle &a, const Rectangle &b) {
    /* determine top-left as maximum of both */
    Point2D tr = {std::max(a.x, b.x), std::max(a.y, b.y)};

    /* determine bottom-right as minimum of both */
    auto abr = a.br(), bbr = b.br();
    Point2D br = {std::min(abr.x, bbr.x), std::min(abr.y, bbr.y)};

    return {tr.x, tr.y, br.x - tr.x, br.y - tr.y};
}

typedef struct RectList {
    Rectangle rect;
    struct RectList* next;
} RectList;

void rectlist_free(RectList* head) {
    if (!head) return;
    RectList* next = head->next;
    g_free(head);
    rectlist_free(next);
}

static int rectlist_length_acc(RectList* head, int acc) {
    if (!head) return acc;
    else return rectlist_length_acc(head->next, acc + 1);
}

int rectlist_length(RectList* head) {
    return rectlist_length_acc(head, 0);
}

static RectList* rectlist_create_simple(int x1, int y1, int x2, int y2) {
    if (x1 >= x2 || y1 >= y2) {
        return nullptr;
    }
    RectList* r = g_new0(RectList, 1);
    r->rect.x = x1;
    r->rect.y = y1;
    r->rect.width  = x2 - x1;
    r->rect.height = y2 - y1;
    r->next = nullptr;
    return r;
}

// forward decl for circular calls
RectList* reclist_insert_disjoint(RectList* head, RectList* element);

static RectList* insert_rect_border(RectList* head,
                                    Rectangle large, Rectangle center)
{
    // given a large rectangle and a center which guaranteed to be a subset of
    // the large rect, the task is to split "large" into pieces and insert them
    // like this:
    //
    // +------- large ---------+
    // |         top           |
    // |------+--------+-------|
    // | left | center | right |
    // |------+--------+-------|
    // |        bottom         |
    // +-----------------------+
    RectList *top, *left, *right, *bottom;
    // coordinates of the bottom right corner of large
    int br_x = large.x + large.width, br_y = large.y + large.height;
    RectList* (*r)(int,int,int,int) = rectlist_create_simple;
    top   = r(large.x, large.y, large.x + large.width, center.y);
    left  = r(large.x, center.y, center.x, center.y + center.height);
    right = r(center.x + center.width, center.y, br_x, center.y + center.height);
    bottom= r(large.x, center.y + center.height, br_x, br_y);

    RectList* parts[] = { top, left, right, bottom };
    for (unsigned int i = 0; i < LENGTH(parts); i++) {
        head = reclist_insert_disjoint(head, parts[i]);
    }
    return head;
}

// insert a new element without any intersections into the given list
RectList* reclist_insert_disjoint(RectList* head, RectList* element) {
    if (!element) {
        return head;
    } else if (!head) {
        // if the list is empty, then intersection-free insertion is trivial
        element->next = nullptr;
        return element;
    } else if (!rects_intersect(head->rect, element->rect)) {
        head->next = reclist_insert_disjoint(head->next, element);
        return head;
    } else {
        // element intersects with the head rect
        auto center = intersection_area(head->rect, element->rect);
        auto large = head->rect;
        head->rect = center;
        head->next = insert_rect_border(head->next, large, center);
        head->next = insert_rect_border(head->next, element->rect, center);
        g_free(element);
        return head;
    }
}

RectangleVec disjoin_rects(const RectangleVec &buf) {
    RectList* cur;
    struct RectList* rects = nullptr;
    for (auto& rect : buf) {
        cur = g_new0(RectList, 1);
        cur->rect = rect;
        rects = reclist_insert_disjoint(rects, cur);
    }
    cur = rects;
    RectangleVec ret(rectlist_length(rects));
    FOR (i,0,ret.size()) {
        ret[i] = cur->rect;
        cur = cur->next;
    }
    rectlist_free(rects);
    return ret;
}
/* end of TODO to remove RectList */

int disjoin_rects_command(Input input, Output output) {
    if (input.empty()) {
        return HERBST_NEED_MORE_ARGS;
    }

    RectangleVec rects;
    for (auto &i : input) {
        rects.push_back(Rectangle::fromStr(i));
    }

    for (auto &r : disjoin_rects(rects)) {
        output << r << endl;
    }
    return 0;
}
