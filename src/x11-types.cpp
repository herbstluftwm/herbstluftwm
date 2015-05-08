#include "globals.h"
#include "x11-types.h"
#include "glib-backports.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <iostream>
#include <iomanip>

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
	convert(source, dest);
	return dest;
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
