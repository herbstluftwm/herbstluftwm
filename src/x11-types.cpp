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
    stringstream ss;
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
    if (!success) {
        throw std::invalid_argument(
                string("cannot allocate color \'") + payload + "\'");
    }

    return Color(ret_color);
}

XColor Color::toXColor() const {
    return XColor{x11pixelValue_, red_, green_, blue_, DoRed | DoGreen | DoBlue, 0};
}



int Point2D::manhattanLength() const
{
    return std::abs(x) + std::abs(y);
}
