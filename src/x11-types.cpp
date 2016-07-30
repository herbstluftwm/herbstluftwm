#include "globals.h"
#include "x11-types.h"
#include "glib-backports.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <iostream>
#include <iomanip>
#include <cstdio>

namespace herbstluft {

// get X11 color from color string
// from dwm.c
bool Color::convert(const char *source, Color& dest) {
	Colormap cmap = DefaultColormap(g_display, g_screen);
	XColor color;
	if(!XAllocNamedColor(g_display, cmap, source, &color, &color)) {
		g_warning("error, cannot allocate color '%s'\n", source);
		return false;
	}
	dest = color.pixel;
	//dest.name_ = source;
	return true;
}

Color Color::fromStr(const char *source) {
	Color dest{0}; // initialize in case convert() fails
	//convert(source, dest);
	unsigned long val = 0;
	if (1 == sscanf(source, "#%lx", &val)) {
		dest.value_ = val;
	}
	return dest;
}

Color Color::fromStr(const std::string& source) {
    return fromStr(source.c_str());
}

std::string Color::str() const {
	// I do not know how to do this in C++, so do it the
	// C-Style way -- thorsten.
	char tmp_buf[100];
	snprintf(tmp_buf, 100, "#%06lx", value_);
	return std::string(tmp_buf);
}

XColor Color::toXColor() const {
	XColor xcol;
	xcol.pixel = 0;
	unsigned long tmp = value_;
	// xcol.{red,green,blue} are shorts ranging form 0 to 65535 inclusive
	// so we need our format (0 to 255 inclusive) to the full scale
	xcol.red = (tmp & 0xFF) * (65536 / 256);
	tmp /= 256;
	xcol.green = (tmp & 0xFF) * (65536 / 256);
	tmp /= 256;
	xcol.blue = (tmp & 0xFF) * (65536 / 256);
	tmp /= 256;
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
