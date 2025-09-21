#ifndef __HERBST_X11_TYPES_H_
#define __HERBST_X11_TYPES_H_

#include <X11/Xlib.h>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "converter.h"

class XConnection;

class Color {
public:
    Color();
    Color(XColor xcol, unsigned short alpha = 0xff);
    Color(std::string name);

    static Color black();

    // throws std::invalid_argument
    static Color fromStr(const std::string& payload);
    std::string str() const;

    // return an XColor as obtained form XQueryColor
    XColor toXColor() const;

    bool operator==(const Color& other) const {
        return red_ == other.red_
            && green_ == other.green_
            && blue_ == other.blue_
            && alpha_ == other.alpha_;
    };
    bool operator!=(const Color& other) const {
        return !operator==(other);
    }

    // use the X-style definition of colors:
    // each of the color components is a value
    // in the range 0 to 65535 inclusive. (all 0 means black, all 65535 is
    // white)
    unsigned short red_ = 0;
    unsigned short green_ = 0;
    unsigned short blue_ = 0;
    unsigned short alpha_ = 0xff; // 0 is fully transparent, 0xff is fully opaque

private:
    // the x11 internal pixel value
    unsigned long x11pixelValue_ = 0;
};

template<>
inline std::string Converter<Color>::str(Color payload) { return payload.str(); }

template<>
inline Color Converter<Color>::parse(const std::string &payload) {
    return Color::fromStr(payload);
}
// TODO: a parse() with relative modifiers, ie brightness+10/red-20 would be cool

struct Point2D {
    int x;
    int y;
    static inline Point2D fold(std::function<int(int,int)> oper, const std::initializer_list<Point2D>& points) {
        bool first = true;
        Point2D result = {0, 0};
        for (const auto& p : points) {
            if (first) {
                result = p;
                first = false;
            } else {
                result.x = oper(result.x, p.x);
                result.y = oper(result.y, p.y);
            }
        }
        return result;
    }
    XPoint toXPoint() const {
        return { static_cast<short>(x), static_cast<short>(y)};
    }
    Point2D operator+(const Point2D& other) const { return { x + other.x, y + other.y }; }
    Point2D operator-(const Point2D& other) const { return { x - other.x, y - other.y }; }
    Point2D operator*(double scalar) const { return { (int) (x * scalar), (int) (y * scalar) }; }
    Point2D operator/(double scalar) const { return { (int) (x / scalar), (int) (y / scalar) }; }
    //! compare w.r.t. lexicographic order
    bool operator<(const Point2D& other) const {
        return x < other.x || (x == other.x && y < other.y);
    }
    bool operator==(const Point2D& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Point2D& other) const { return x != other.x || y != other.y; }
    //! essentially return y/x > other.y/other.x
    bool biggerSlopeThan(const Point2D& other) const {
       return y * other.x > other.y * x;
    }
    int manhattanLength() const;
    inline void set(int new_x, int new_y) { x = new_x; y = new_y; }
};


//! utility functions for the Window type
class WindowID {
public:
    WindowID(Window w) : value_(w) { }
    inline Window operator()() const { return value_; }
    inline std::string str() const {
        std::stringstream ss;
        ss << "0x" << std::hex << value_;
        return ss.str();
    }
    operator Window() const { return value_; }
private:
    Window value_;
};

/** since WindowID is a new type, we can define a Converter instance for it.
 * (Window clashes with unsigned long)
 */
template<>
inline std::string Converter<WindowID>::str(WindowID payload) {
    return payload.str();
}

template<>
inline WindowID Converter<WindowID>::parse(const std::string &payload) {
    size_t bytes_read = 0;
    unsigned long winid;
    try {
        winid = std::stoul(payload, &bytes_read, 0);
    }  catch (...) {
        throw std::invalid_argument("Window id is not numeric (decimal or 0xHEX)");
    }
    if (bytes_read != payload.size()) {
        std::stringstream message;
        message << "invalid characters at position " << bytes_read
            << " of \"" << payload << "\"";
        throw std::invalid_argument(message.str());
    }
    return winid;
}

#endif

