#include "rectangle.h"

#include <X11/Xutil.h>
#include <limits>
#include <queue>

#include "ipc-protocol.h"
#include "utils.h"

using std::endl;
using std::string;
using std::vector;


Rectangle Rectangle::fromStr(const string &source) {
    int x, y;
    unsigned int w, h;
    int flags = XParseGeometry(source.c_str(), &x, &y, &w, &h);

    return {
        (XValue & flags) ? x : 0,
        (YValue & flags) ? y : 0,
        (WidthValue & flags) ? (int)w : 0,
        (HeightValue & flags) ? (int)h : 0
    };
}

Rectangle Rectangle::fromCorners(Point2D tl_, Point2D br_)
{
    return fromCorners(tl_.x, tl_.y, br_.x, br_.y);
}

Rectangle Rectangle::fromCorners(int x1, int y1, int x2, int y2) {
    Rectangle r;
    r.x = x1;
    r.y = y1;
    r.width  = x2 - x1;
    r.height = y2 - y1;
    return r;
}

Rectangle Rectangle::adjusted(int dx, int dy) const
{
    return adjusted(dx, dy, dx, dy);
}

Rectangle Rectangle::adjusted(int left, int top, int right, int bottom) const
{
    return {x - left, y - top, width + left + right, height + top + bottom};
}

//! lexicographic order (wrt x,y,width,height)
bool Rectangle::operator<(const Rectangle& other) const
{
    if (x != other.x) {
        return x < other.x;
    }
    if (y != other.y) {
        return y < other.y;
    }
    if (width != other.width) {
        return width < other.width;
    }
    if (height != other.height) {
        return height < other.height;
    }
    return false;
}

bool Rectangle::operator==(const Rectangle& other) const
{
    return x == other.x
        && y == other.y
        && width == other.width
        && height == other.height;
}

/**
 * @brief Check whether a rectangle has positive width and height
 */
Rectangle::operator bool() const
{
    return (width > 0) && (height > 0);
}

/**
 * @brief Return the intersection with another rectangel
 * @param the other rectangle
 * @return the intersection
 */
Rectangle Rectangle::intersectionWith(const Rectangle &other) const
{
    return Rectangle::fromCorners(
                std::max(x, other.x),
                std::max(y, other.y),
                std::min(br().x, other.br().x),
                std::min(br().y, other.br().y));
}

//! the minimum distance between any two points from the one and the other
//! rectangle
int Rectangle::manhattanDistanceTo(Rectangle &other) const
{
    if (intersectionWith(other)) {
        return 0; // distance 0 if there is an intersection
    }
    // if they don't intersect but are below each other
    if (intervals_intersect(x, x + width, other.x, other.x + other.width)) {
        return std::min(
                    // this is below the other:
                    std::abs(y - (other.y + other.height)),
                    // this is above the other:
                    std::abs(other.y - (y + height)));
    }
    // if they don't intersect but are next to each other
    if (intervals_intersect(y, y + height, other.y, other.y + other.height)) {
        return std::min(
                    // this is right of the other
                    std::abs(x - (other.x + other.width)),
                    // this is left of the other
                    std::abs(other.x - (x + width)));
    }
    vector<Point2D> thisCorners = { tl(), tr(), bl(), br() };
    vector<Point2D> otherCorners = { other.tl(), other.tr(), other.bl(), other.br() };
    int minDist = std::numeric_limits<int>::max();
    for (const auto& p1 : thisCorners) {
        for (const auto& p2 : otherCorners) {
            minDist = std::min(minDist, (p1 - p2).manhattanLength());
        }
    }
    return minDist;
}

std::ostream& operator<< (std::ostream& stream, const Rectangle& rect) {
    stream
        << rect.width << "x" << rect.height
        << std::showpos
        << rect.x << rect.y
        << std::noshowpos;
    return stream;
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
    auto r = [](int x1, int y1, int x2, int y2) {
        return Rectangle::fromCorners(x1, y1, x2, y2);
    };
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
            if (result[i].intersectionWith(rectToInsert)) {
                break;
            }
        }
        if (i >= result.size()) {
            // no intersection means, we can insert it without any issue
            result.push_back(rectToInsert);
        } else {
            // if there's an intersection
            Rectangle center = result[i].intersectionWith(rectToInsert);
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
