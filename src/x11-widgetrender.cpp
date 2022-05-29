#include "x11-widgetrender.h"

#include <vector>

#include "css.h"
#include "rectangle.h"
#include "widget.h"
#include "xconnection.h"

using std::vector;

X11WidgetRender::X11WidgetRender(Pixmap& pixmap, Point2D pixmapPos,
                                 Colormap& colormap, GC& gc)
    : xcon_(XConnection::get())
    , pixmap_(pixmap)
    , pixmapPos_(pixmapPos)
    , colormap_(colormap)
    , gc_(gc)
{
}

void X11WidgetRender::render(const Widget& widget)
{
    Rectangle geo = widget.geometryCached().shifted(pixmapPos_ * -1);
    const BoxStyle& style = widget.style_
            ? *widget.style_
            : BoxStyle::empty;
    fillRectangle(geo, style.backgroundColor);
    Color bcol("red");
    vector<Rectangle> borderRects = {
        {geo.x, geo.y, style.borderWidthLeft, geo.height},
        {geo.x, geo.y, geo.width, style.borderWidthTop},
        {geo.x + geo.width - style.borderWidthRight, geo.y,
         style.borderWidthRight, geo.height},
        {geo.x, geo.y + geo.height - style.borderWidthBottom,
         geo.width, style.borderWidthBottom},
    };
    for (const auto& r : borderRects) {
        fillRectangle(r, bcol);
    }
    for (const Widget* child : widget.nestedWidgets_) {
        render(*child);
    }
}

void X11WidgetRender::fillRectangle(Rectangle rect, const Color& color)
{
    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }
    XSetForeground(xcon_.display(),
                   gc_,
                   xcon_.allocColor(colormap_, color));
    XFillRectangle(xcon_.display(),
                   pixmap_, gc_,
                   rect.x, rect.y,
                   static_cast<unsigned int>(rect.width),
                   static_cast<unsigned int>(rect.height));
}
