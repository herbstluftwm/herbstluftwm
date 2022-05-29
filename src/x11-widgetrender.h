#ifndef X11WIDGETRENDER_H
#define X11WIDGETRENDER_H

#include <X11/X.h>
#include <X11/Xlib.h>

#include "rectangle.h"
#include "x11-types.h"

class Widget;
class FontData;
class Settings;
enum class TextAlign;

class X11WidgetRender
{
public:
    X11WidgetRender(Settings& settings, Pixmap& pixmap, Point2D pixmapPos,
                    Colormap& colormap, GC& gc_, Visual* visual);
    void render(const Widget& widget);
private:
    inline void fillRectangle(Rectangle rect, const Color& color);

    void drawText(Pixmap& pix, GC& gc, const FontData& fontData,
                  const Color& color, Point2D position, const std::string& text,
                  int width, const TextAlign& align );


    XConnection& xcon_;
    Settings& settings_;
    Pixmap& pixmap_;
    Point2D pixmapPos_;
    Colormap& colormap_;
    GC& gc_;
    Visual* visual_;
};

#endif // X11WIDGETRENDER_H
