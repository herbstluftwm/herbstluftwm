#ifndef __HERBSTLUFT_RECTANGLE_H_
#define __HERBSTLUFT_RECTANGLE_H_

#include <string>

#include "commandio.h"
#include "converter.h"
#include "x11-types.h"

struct Rectangle {
    //! Construct a default rectangle (0/0/0/0)
    Rectangle() : x(0), y(0), width(0), height(0) {}

    Rectangle(int x_, int y_, int width_, int height_)
        : x(x_), y(y_), width(width_), height(height_) {}

    static Rectangle fromStr(const std::string &source);

    static Rectangle fromCorners(int x1, int y1, int x2, int y2);

    Point2D tl() const { return {x, y}; }
    Point2D br() const { return {x + width, y + height}; }
    Point2D bl() const { return {x, y + height}; }
    Point2D tr() const { return {x + width, y}; }
    Point2D dimensions() const { return { width, height}; }

    //! Grow/shrink by dx left and right, by dy top and bottom, respectively
    Rectangle adjusted(int dx, int dy) const;
    //! Grow/shrink in each of the four given directions
    Rectangle adjusted(int left, int top, int right, int bottom) const;

    bool operator<(const Rectangle& other) const;
    bool operator==(const Rectangle& other) const;
    bool operator!=(const Rectangle& other) const { return !(this->operator==(other));}

    operator bool() const;

    Rectangle intersectionWith(const Rectangle& other) const;
    int manhattanDistanceTo(Rectangle& other) const;

    int x;
    int y;
    int width;
    int height;
};
std::ostream& operator<< (std::ostream& stream, const Rectangle& rect);

template<>
inline std::string Converter<Rectangle>::str(Rectangle payload) {
    std::stringstream ss;
    ss << payload;
    return ss.str();
}

template<>
inline Rectangle Converter<Rectangle>::parse(const std::string &payload) {
    return Rectangle::fromStr(payload);
}
// TODO: a parse() with relative modifiers, ie a syntax for shifts, might be cool

using RectangleVec = std::vector<Rectangle>;
using RectangleIdxVec = std::vector<std::pair<int, Rectangle>>;

RectangleVec disjoin_rects(const RectangleVec &buf);
int disjoin_rects_command(Input input, Output output);

#endif

