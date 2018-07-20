
#include "rectangle.h"
#include "floating.h"
#include "mouse.h"
#include "ipc-protocol.h"
#include <glib.h>

static bool rects_intersect(RectList* m1, RectList* m2) {
    Rectangle *r1 = &m1->rect, *r2 = &m2->rect;
    bool is = TRUE;
    is = is && intervals_intersect(r1->x, r1->x + r1->width,
                                   r2->x, r2->x + r2->width);
    is = is && intervals_intersect(r1->y, r1->y + r1->height,
                                   r2->y, r2->y + r2->height);
    return is;
}

static Rectangle intersection_area(RectList* m1, RectList* m2) {
    Rectangle r; // intersection between m1->rect and m2->rect
    r.x = std::max(m1->rect.x, m2->rect.x);
    r.y = std::max(m1->rect.y, m2->rect.y);
    // the bottom right coordinates of the rects
    int br1_x = m1->rect.x + m1->rect.width;
    int br1_y = m1->rect.y + m1->rect.height;
    int br2_x = m2->rect.x + m2->rect.width;
    int br2_y = m2->rect.y + m2->rect.height;
    r.width = std::min(br1_x, br2_x) - r.x;
    r.height = std::min(br1_y, br2_y) - r.y;
    return r;
}

static RectList* rectlist_create_simple(int x1, int y1, int x2, int y2) {
    if (x1 >= x2 || y1 >= y2) {
        return NULL;
    }
    RectList* r = g_new0(RectList, 1);
    r->rect.x = x1;
    r->rect.y = y1;
    r->rect.width  = x2 - x1;
    r->rect.height = y2 - y1;
    r->next = NULL;
    return r;
}

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
        element->next = NULL;
        return element;
    } else if (!rects_intersect(head, element)) {
        head->next = reclist_insert_disjoint(head->next, element);
        return head;
    } else {
        // element intersects with the head rect
        auto center = intersection_area(head, element);
        auto large = head->rect;
        head->rect = center;
        head->next = insert_rect_border(head->next, large, center);
        head->next = insert_rect_border(head->next, element->rect, center);
        g_free(element);
        return head;
    }
}

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

RectList* disjoin_rects(const RectangleVec &buf) {
    RectList* cur;
    struct RectList* rects = NULL;
    for (auto& rect : buf) {
        cur = g_new0(RectList, 1);
        cur->rect = rect;
        rects = reclist_insert_disjoint(rects, cur);
    }
    return rects;
}


int disjoin_rects_command(int argc, char** argv, Output output) {
    (void)SHIFT(argc, argv);
    if (argc < 1) {
        return HERBST_NEED_MORE_ARGS;
    }
    RectangleVec buf(argc);
    for (int i = 0; i < argc; i++) {
        buf[i] = Rectangle::fromStr(argv[i]);
    }

    RectList* rects = disjoin_rects(buf);
    for (RectList* cur = rects; cur; cur = cur->next) {
        Rectangle &r = cur->rect;
        output << r << std::endl;
    }
    rectlist_free(rects);
    return 0;
}
