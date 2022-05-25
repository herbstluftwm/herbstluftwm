#ifndef X11WIDGETRENDER_H
#define X11WIDGETRENDER_H

#include <X11/X.h>
#include <X11/Xlib.h>

#include "rectangle.h"
#include "x11-types.h"

class Widget;

class X11WidgetRender
{
public:
    X11WidgetRender(Pixmap& pixmap, Point2D pixmapPos, Colormap& colormap, GC& gc_);
    void render(const Widget& widget);
private:
    inline void fillRectangle(Rectangle rect, const Color& color);
    XConnection& xcon_;
    Pixmap& pixmap_;
    Point2D pixmapPos_;
    Colormap& colormap_;
    GC& gc_;
};

#endif // X11WIDGETRENDER_H
