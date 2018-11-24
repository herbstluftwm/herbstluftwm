#include "globals.h"
#include "x11-types.h"
#include "glib-backports.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <iostream>
#include <iomanip>
#include <cstdio>
#include <sstream>


// get X11 color from color string. This fails if there is no x connection
// from dwm.c
std::string queryX11Color(const char *source, XColor& dest_color) {
	if (!g_display) {
		return "g_display is not set";
	}
	Colormap cmap = DefaultColormap(g_display, g_screen);
	XColor screen_color;
	if(!XAllocNamedColor(g_display, cmap, source, &screen_color, &dest_color)) {
		return std::string("cannot allocate color \'") + source + "\'";
	}
	//dest.name_ = source;
	return "";
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
    if ("" != fromStr(name, *this)) {
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

std::string Color::fromStr(const std::string& source, Color& target) {
    XColor xcol;
    std::string msg = queryX11Color(source.c_str(), xcol);
    if (msg != "") {
        return msg;
    } else {
        // TODO: how to interpret these fields if
        // xcol.flags lacks one of DoRed, DoGreen, DoBlue?
        target.red_ = xcol.red;
        target.green_ = xcol.green;
        target.blue_ = xcol.blue;
        target.x11pixelValue_ = xcol.pixel;
        return "";
    }
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

Rectangle Rectangle::fromStr(const std::string &source) {
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

std::ostream& operator<< (std::ostream& stream, const Rectangle& rect) {
    stream
        << rect.width << "x" << rect.height
        << std::showpos
        << rect.x << rect.y
        << std::noshowpos;
    return stream;
}

