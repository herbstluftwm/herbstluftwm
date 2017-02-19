#ifndef __HERBST_X11_TYPES_H_
#define __HERBST_X11_TYPES_H_

#include <vector>
#include <string>
#include <iostream>
#include <memory>
#include <X11/Xlib.h>

#define Ptr(X) std::shared_ptr<X>
#define WPtr(X) std::weak_ptr<X>


class Color {
public:
    Color();
    Color(std::string name);

    static Color fromStr(const char *source);
    static Color fromStr(const std::string& source);
    static Color black();

    std::string str() const;

    // return an XColor as obtained form XQueryColor
    XColor toXColor() const;
    unsigned long toX11Pixel() const { return x11pixelValue_; }

private:
    // use the X-style definition of colors:
    // each of the color components is a value
    // in the range 0 to 65535 inclusive. (all 0 means black, all 65535 is
    // white)
    unsigned short red_, green_, blue_;

    // the x11 internal pixel value.
    unsigned long x11pixelValue_;
};

struct Rectangle {
    static Rectangle fromStr(const char *source);

    int x;
    int y;
    int width;
    int height;
};

using RectangleVec = std::vector<Rectangle>;

struct Point2D {
    int x;
    int y;
};

std::ostream& operator<< (std::ostream& stream, const Rectangle& matrix);


#endif

