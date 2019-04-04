#include "rectangle.h"

#include <queue>

#include "ipc-protocol.h"
#include "utils.h"

using std::endl;
using std::vector;

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

static RectangleVec disjoin_from_subset(Rectangle large, Rectangle center)
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
    // coordinates of the bottom right corner of large
    int br_x = large.x + large.width, br_y = large.y + large.height;
    auto r = Rectangle::fromCorners;
    Rectangle top   = r(large.x, large.y, large.x + large.width, center.y);
    Rectangle left  = r(large.x, center.y, center.x, center.y + center.height);
    Rectangle right = r(center.x + center.width, center.y, br_x, center.y + center.height);
    Rectangle bottom= r(large.x, center.y + center.height, br_x, br_y);

    RectangleVec parts = { top, left, right, bottom };
    RectangleVec res;
    for (auto& rect : parts ) {
        if (rect.width <= 0 || rect.height <= 0) {
            continue;
        }
        res.push_back(rect);
    }
    return res;
}

RectangleVec disjoin_rects(const RectangleVec &buf) {
    std::queue<Rectangle> q; // rectangles not inserted yet
    for (const auto& r : buf) {
        q.push(r);
    }
    RectangleVec result; // rectangles that are disjoint
    while (!q.empty()) {
        auto rectToInsert = q.front();
        q.pop();
        // find some rectangle in 'result' that intersects with rectToInsert
        size_t i;
        for (i = 0; i < result.size(); i++) {
            if (rects_intersect(result[i], rectToInsert)) {
                break;
            }
        }
        if (i >= result.size()) {
            // no intersection means, we can insert it without any issue
            result.push_back(rectToInsert);
        } else {
            // if there's an intersection
            Rectangle center = intersection_area(result[i], rectToInsert);
            // then cut both rectangles into pieces according to the intersection:
            // 1. all rectangles of 'result' are invariantly disjoint, and so we can add them
            // to the result
            for (const auto& r : disjoin_from_subset(result[i], center)) {
                result.push_back(r);
            }
            // and we can replace result[i] by the intersection
            result[i] = center;
            // 2. the other pieces of rectToInsert possibly intersect with other rectangles,
            // so process them later
            for (const auto& r : disjoin_from_subset(rectToInsert, center)) {
                q.push(r);
            }
        }
    }
    return result;
}

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
