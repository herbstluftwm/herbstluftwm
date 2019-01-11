#ifndef __HERBST_X11_TYPES_H_
#define __HERBST_X11_TYPES_H_

#include <X11/Xlib.h>
#include <iostream>
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

private:
    // use the X-style definition of colors:
    // each of the color components is a value
    // in the range 0 to 65535 inclusive. (all 0 means black, all 65535 is
    // white)
    unsigned short red_ = 0;
    unsigned short green_ = 0;
    unsigned short blue_ = 0;

    // the x11 internal pixel value.
    unsigned long x11pixelValue_ = 0;
};

template<>
inline std::string Converter<Color>::str(Color payload) { return payload.str(); }

template<>
inline Color Converter<Color>::parse(const std::string &payload, Color const*) {
    // TODO: relative modifiers, ie brightness+10/red-20 would be cool
    return Color::fromStr(payload);
}

struct Point2D {
    int x;
    int y;
};

struct Rectangle {
    static Rectangle fromStr(const std::string &source);

    Point2D tl() const { return {x, y}; }
    Point2D br() const { return {x + width, y + height}; }

    //! Grow/shrink by dx left and right, by dy top and bottom, respectively
    Rectangle adjusted(int dx, int dy) const;
    //! Grow/shrink in each of the four given directions
    Rectangle adjusted(int left, int top, int right, int bottom) const;

    int x;
    int y;
    int width;
    int height;
};
std::ostream& operator<< (std::ostream& stream, const Rectangle& matrix);

template<>
inline std::string Converter<Rectangle>::str(Rectangle payload) {
    std::stringstream ss;
    ss << payload;
    return ss.str();
}

template<>
inline Rectangle Converter<Rectangle>::parse(const std::string &payload, Rectangle const*) {
    // TODO: relative modifiers, ie a syntax for shifts, might be cool
    return Rectangle::fromStr(payload);
}

using RectangleVec = std::vector<Rectangle>;
using RectangleIdxVec = std::vector<std::pair<int, Rectangle>>;

#endif

