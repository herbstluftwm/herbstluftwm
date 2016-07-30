#ifndef __HERBST_X11_TYPES_H_
#define __HERBST_X11_TYPES_H_

#include <vector>
#include <string>
#include <iostream>
#include <memory>
#include <X11/Xlib.h>

#define Ptr(X) std::shared_ptr<X>
#define WPtr(X) std::weak_ptr<X>

namespace herbstluft {

    struct Color {
        //Color() : value_(0) {}
        //Color(unsigned long value) : value_(value) {}

        static bool convert(const char *source, Color& dest);
        static Color fromStr(const char *source);
        static Color fromStr(const std::string& source);

        std::string str() const;

        operator unsigned long() const { return value_; }
        void operator=(unsigned long value) { value_ = value; }
        // return an XColor as obtained form XQueryColor
        XColor toXColor() const;

        unsigned long value_; // saved as rgb (1 byte each)
        //std::string name_;
    };

    struct Rectangle {
        static Rectangle fromStr(const char *source);

        int x;
        int y;
        unsigned int width;
        unsigned int height;
    };
    using RectangleVec = std::vector<Rectangle>;

    struct Point2D {
        int x;
        int y;
    };

    std::ostream& operator<< (std::ostream& stream, const Rectangle& matrix);
}

#endif

