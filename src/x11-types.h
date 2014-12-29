#ifndef __HERBST_X11_TYPES_H_
#define __HERBST_X11_TYPES_H_

#include <vector>
#include <string>

namespace herbstluft {

    struct Color {
        //Color() : value_(0) {}
        //Color(unsigned long value) : value_(value) {}

        // implemented in utils.h, should be moved
        static bool convert(const char *source, Color& dest);
        static Color fromStr(const char *source) {
            Color dest{0}; // initialize in case convert() fails
            convert(source, dest);
            return dest;
        }

        std::string str() {
        //    if (name_.empty())
                return std::to_string(value_);
        //    else
        //        return name_;
        }

        operator unsigned long() { return value_; }
        void operator=(unsigned long value) { value_ = value; }

        unsigned long value_;
        //std::string name_;
    };

    struct Rectangle {
        // implemented in utils.h, should be moved
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

}

#endif

