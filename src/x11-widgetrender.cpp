#include "x11-widgetrender.h"

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <vector>

#include "css.h"
#include "rectangle.h"
#include "fontdata.h"
#include "settings.h"
#include "utils.h"
#include "widget.h"
#include "xconnection.h"

using std::string;
using std::vector;

X11WidgetRender::X11WidgetRender(Settings& settings, Pixmap& pixmap, Point2D pixmapPos,
                                 Colormap& colormap, GC& gc, Visual* visual)
    : xcon_(XConnection::get())
    , settings_(settings)
    , pixmap_(pixmap)
    , pixmapPos_(pixmapPos)
    , colormap_(colormap)
    , gc_(gc)
    , visual_(visual)
{
}

void X11WidgetRender::render(const Widget& widget)
{
    const BoxStyle& style = widget.style_
            ? *widget.style_
            : BoxStyle::empty();
    Rectangle geo =
            widget
            .geometryCached()
            .shifted(pixmapPos_ * -1)
            .adjusted(-style.marginLeft,
                      -style.marginTop,
                      -style.marginRight,
                      -style.marginBottom);
    fillRectangle(geo, style.backgroundColor);
    vector<Rectangle> borderRects = {
        {geo.x, geo.y, style.borderWidthLeft, geo.height},
        {geo.x, geo.y, geo.width, style.borderWidthTop},
        {geo.x + geo.width - style.borderWidthRight, geo.y,
         style.borderWidthRight, geo.height},
        {geo.x, geo.y + geo.height - style.borderWidthBottom,
         geo.width, style.borderWidthBottom},
    };
    for (const auto& r : borderRects) {
        if (r) {
            fillRectangle(r, style.borderColor);
        }
    }
    for (const Widget* child : widget.nestedWidgets_) {
        render(*child);
    }
    if (widget.hasText_) {
        Rectangle contentGeo = widget.contentGeometryCached();
        Point2D textPos = contentGeo.tl();
        int extraSpace = contentGeo.height - style.textHeight - style.textDepth;
        textPos.y += extraSpace / 2 + style.textHeight;
        drawText(pixmap_, gc_, style.font.data(), style.fontColor,
                 textPos - pixmapPos_, widget.textContent(), geo.width, style.textAlign);
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

/**
 * @brief Draw a given text
 * @param pix The pixmap
 * @param gc The graphic context
 * @param fontData
 * @param color
 * @param position The position of the left end of the baseline
 * @param width The maximum width of the string (in pixels)
 * @param the horizontal alignment within this maximum width
 * @param text
 */
void X11WidgetRender::drawText(Pixmap& pix, GC& gc, const FontData& fontData, const Color& color,
                          Point2D position, const string& text, int width,
                          const TextAlign& align)
{
    Display* display = xcon_.display();
    // shorten the text first:
    size_t textLen = text.size();
    int textwidth = fontData.textwidth(text, textLen);
    string with_ellipsis; // declaration here for sufficently long lifetime
    const char* final_c_str = nullptr;
    if (textwidth <= width) {
        final_c_str = text.c_str();
    } else {
        // shorten title:
        with_ellipsis = text + settings_.ellipsis();
        // temporarily, textLen is the length of the text surviving from the
        // original window title
        while (textLen > 0 && textwidth > width) {
            textLen--;
            // remove the (multibyte-)character that ends at with_ellipsis[textLen]
            size_t character_width = 1;
            while (textLen > 0 && utf8_is_continuation_byte(with_ellipsis[textLen])) {
                textLen--;
                character_width++;
            }
            // now, textLen points to the first byte of the (multibyte-)character
            with_ellipsis.erase(textLen, character_width);
            textwidth = fontData.textwidth(with_ellipsis, with_ellipsis.size());
        }
        // make textLen refer to the actual string and shorten further if it
        // is still too wide:
        textLen = with_ellipsis.size();
        while (textLen > 0 && textwidth > width) {
            textLen--;
            textwidth = fontData.textwidth(with_ellipsis, textLen);
        }
        final_c_str = with_ellipsis.c_str();
    }
    switch (align) {
    case TextAlign::left: break;
    case TextAlign::center: position.x += (width - textwidth) / 2; break;
    case TextAlign::right: position.x += width - textwidth; break;
    }
    if (fontData.xftFont_) {
        Visual* xftvisual = visual_ ? visual_ : xcon_.visual();
        Colormap xftcmap = colormap_ ? colormap_ : xcon_.colormap();
        XftDraw* xftd = XftDrawCreate(display, pix, xftvisual, xftcmap);
        XRenderColor xrendercol = {
                color.red_,
                color.green_,
                color.blue_,
                // TODO: make xft respect the alpha value
                0xffff, // alpha as set by XftColorAllocName()
        };
        XftColor xftcol = { };
        XftColorAllocValue(display, xftvisual, xftcmap, &xrendercol, &xftcol);
        XftDrawStringUtf8(xftd, &xftcol, fontData.xftFont_,
                       position.x, position.y,
                       (const XftChar8*)final_c_str, textLen);
        XftDrawDestroy(xftd);
        XftColorFree(display, xftvisual, xftcmap, &xftcol);
    } else if (fontData.xFontSet_) {
        XSetForeground(display, gc, xcon_.allocColor(colormap_, color));
        XmbDrawString(display, pix, fontData.xFontSet_, gc, position.x, position.y,
                final_c_str, textLen);
    } else if (fontData.xFontStruct_) {
        XSetForeground(display, gc, xcon_.allocColor(colormap_, color));
        XFontStruct* font = fontData.xFontStruct_;
        XSetFont(display, gc, font->fid);
        XDrawString(display, pix, gc, position.x, position.y,
                final_c_str, textLen);
    }
}
