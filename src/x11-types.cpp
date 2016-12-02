#include "globals.h"
#include "x11-types.h"
#include "glib-backports.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <iostream>
#include <iomanip>
#include <cstdio>
#include <sstream>

namespace herbstluft {

// get X11 color from color string. This fails if there is no x connection
// from dwm.c
bool queryX11Color(const char *source, XColor& dest_color) {
	if (!g_display) {
		return false;
	}
	Colormap cmap = DefaultColormap(g_display, g_screen);
	XColor screen_color;
	if(!XAllocNamedColor(g_display, cmap, source, &screen_color, &dest_color)) {
		g_warning("error, cannot allocate color '%s'\n", source);
		return false;
	}
	//dest.name_ = source;
	return true;
}

Color Color::black() {
    // currently, the constructor without arguments constructs black
    Color c;
    return c;
}

Color::Color()
    : red_(0)
    , green_(0)
    , blue_(0)
    , x11pixelValue_(0)
{
}

Color::Color(std::string name) {
    XColor xcol;
    if (queryX11Color(name.c_str(), xcol)) {
        // TODO: how to interpret these fields if
        // xcol.flags lacks one of DoRed, DoGreen, DoBlue?
        red_ = xcol.red;
        green_ = xcol.green;
        blue_ = xcol.blue;
        x11pixelValue_ = xcol.pixel;
    } else {
        *this = black();
    }
}

std::string Color::str() const {
    unsigned long divisor =  (65536 + 1) / (0xFF + 1);
    std::stringstream ss;
    ss << "#"
       << std::hex << std::setfill('0') << std::setw(2) << (red_ / divisor)
       << std::hex << std::setfill('0') << std::setw(2) << (green_ / divisor)
       << std::hex << std::setfill('0') << std::setw(2) << (blue_ / divisor)
    ;
    return ss.str();
}

Color Color::fromStr(const char *source) {
	return Color(source);
}
Color Color::fromStr(const std::string& source) {
	return Color(source);
}

XColor Color::toXColor() const {
	XColor xcol;
	xcol.pixel = x11pixelValue_;
    xcol.red = red_;
    xcol.green = green_;
    xcol.blue = blue_;
	xcol.flags = DoRed | DoGreen | DoBlue;
	return xcol;
}

Rectangle Rectangle::fromStr(const char* source) {
	int x, y;
	unsigned int w, h;
	int flags = XParseGeometry(source, &x, &y, &w, &h);

	return {
		(XValue & flags) ? x : 0,
		(YValue & flags) ? y : 0,
		(WidthValue & flags) ? w : 0,
		(HeightValue & flags) ? h : 0
	};
}

std::ostream& operator<< (std::ostream& stream, const Rectangle& rect) {
    stream
        << rect.width << "x" << rect.height
        << std::showpos
        << rect.x << rect.y
        << std::noshowpos;
    return stream;
}

}
