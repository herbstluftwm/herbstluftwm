#include "x11-types.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <algorithm>
#include <cassert>
#include <iomanip>
#include <limits>

#include "globals.h"
#include "utils.h"

using std::string;
using std::vector;

Color Color::black() {
    // currently, the constructor without arguments constructs black
    return {};
}

Color::Color()
    : red_(0), green_(0), blue_(0), x11pixelValue_(0)
{
}

Color::Color(XColor xcol)
    : red_(xcol.red), green_(xcol.green), blue_(xcol.blue), x11pixelValue_(xcol.pixel)
{
    // TODO: special interpretation of red, green, blue when
    // xcol.flags lacks one of DoRed, DoGreen, DoBlue?
}

Color::Color(string name) {
    try {
        *this = fromStr(name);
    } catch (...) {
        *this = black();
    }
}

string Color::str() const {
    unsigned long divisor =  (65536 + 1) / (0xFF + 1);
    std::stringstream ss;
    ss << "#"
       << std::hex << std::setfill('0') << std::setw(2) << (red_ / divisor)
       << std::hex << std::setfill('0') << std::setw(2) << (green_ / divisor)
       << std::hex << std::setfill('0') << std::setw(2) << (blue_ / divisor)
    ;
    return ss.str();
}

Color Color::fromStr(const string& payload) {
    // get X11 color from color string. This fails if there is no x connection
    // from dwm.c
    assert(g_display);
    Colormap cmap = DefaultColormap(g_display, g_screen);
    XColor screen_color, ret_color;
    auto success = XAllocNamedColor(g_display, cmap,
                                    payload.c_str(), &screen_color, &ret_color);
    if (!success)
        throw std::invalid_argument(
                string("cannot allocate color \'") + payload + "\'");

    return Color(ret_color);
}

XColor Color::toXColor() const {
    return XColor{x11pixelValue_, red_, green_, blue_, DoRed | DoGreen | DoBlue, 0};
}

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
    if (x != other.x) return x < other.x;
    if (y != other.y) return y < other.y;
    if (width != other.width) return width < other.width;
    if (height != other.height) return height < other.height;
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
 * @brief Check whether a rectangle has non-negative width and height
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


int Point2D::manhattanLength() const
{
    return std::abs(x) + std::abs(y);
}
