#ifndef __HERBST_X11_TYPES_H_
#define __HERBST_X11_TYPES_H_

#include <X11/Xlib.h>
#include <sstream>
#include <string>
#include <vector>

#include "types.h"

class Color {
public:
    Color();
    Color(XColor xcol);
    Color(std::string name);

    static Color black();

    // throws std::invalid_argument
    static Color fromStr(const std::string& payload);
    std::string str() const;

    // return an XColor as obtained form XQueryColor
    XColor toXColor() const;
    unsigned long toX11Pixel() const { return x11pixelValue_; }

    bool operator==(const Color& other) const {
        return red_ == other.red_
            && green_ == other.green_
            && blue_ == other.blue_;
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

private:

    // the x11 internal pixel value.
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
    Point2D operator+(const Point2D& other) const { return { x + other.x, y + other.y }; }
    Point2D operator-(const Point2D& other) const { return { x - other.x, y - other.y }; }
    Point2D operator*(double scalar) const { return { (int) (x * scalar), (int) (y * scalar) }; }
    Point2D operator/(double scalar) const { return { (int) (x / scalar), (int) (y / scalar) }; }
    //! compare w.r.t. lexicographic order
    bool operator<(const Point2D& other) const {
        return x < other.x || (x == other.x && y < other.y);
    }
    bool operator==(const Point2D& other) const { return x == other.x && y == other.y; }
    //! essentially return y/x > other.y/other.x
    bool biggerSlopeThan(const Point2D& other) const {
       return y * other.x > other.y * x;
    }
    int manhattanLength() const;
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
    unsigned long winid = std::stoul(payload, &bytes_read, 0);
    if (bytes_read != payload.size()) {
        std::stringstream message;
        message << "invalid characters at position " << bytes_read
            << " of \"" << payload << "\"";
        throw std::invalid_argument(message.str());
    }
    return winid;
}

#endif

