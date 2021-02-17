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
using std::stringstream;
using std::vector;

Color Color::black() {
    // currently, the constructor without arguments constructs black
    return {};
}

Color::Color()
    : red_(0), green_(0), blue_(0), x11pixelValue_(0)
{
}

Color::Color(XColor xcol, unsigned short alpha)
    : red_(xcol.red), green_(xcol.green), blue_(xcol.blue), alpha_(alpha),
      x11pixelValue_(xcol.pixel)
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
    stringstream ss;
    ss << "#"
       << std::hex << std::setfill('0') << std::setw(2) << (red_ / divisor)
       << std::hex << std::setfill('0') << std::setw(2) << (green_ / divisor)
       << std::hex << std::setfill('0') << std::setw(2) << (blue_ / divisor)
    ;
    if (alpha_ != 0xff) {
        ss  << std::hex << std::setfill('0') << std::setw(2) << alpha_;
    }
    return ss.str();
}

Color Color::fromStr(const string& payload) {
    // get X11 color from color string. This fails if there is no x connection
    // from dwm.c
    assert(g_display);
    Colormap cmap = DefaultColormap(g_display, g_screen);
    XColor screen_color, ret_color;
    string rgb_str = payload;
    unsigned short alpha = 0xff;
    if (payload.size() == 9 && payload[0] == '#') {
        // if the color has the format '#rrggbbaa'
        rgb_str = payload.substr(0, 7);
        string alpha_str = "0x" + payload.substr(7, 2);
        size_t characters_processed = 0;
        try {
            alpha = std::stoi(alpha_str, &characters_processed, 16);
        } catch(...) {
            throw std::invalid_argument(
                string("invalid alpha value \'") + alpha_str + "\'");
        }
        if (alpha > 0xff || characters_processed != alpha_str.size()) {
            throw std::invalid_argument(
                string("invalid alpha value \'") + alpha_str + "\'");
        }
    }
    auto success = XAllocNamedColor(g_display, cmap,
                                    rgb_str.c_str(), &screen_color, &ret_color);
    if (!success) {
        throw std::invalid_argument(
                string("cannot allocate color \'") + payload + "\'");
    }

    return Color(ret_color, alpha);
}

XColor Color::toXColor() const {
    return XColor{x11pixelValue_, red_, green_, blue_, DoRed | DoGreen | DoBlue, 0};
}



int Point2D::manhattanLength() const
{
    return std::abs(x) + std::abs(y);
}
