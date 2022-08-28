#include "x11-widgetrender.h"

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <vector>

#include "css.h"
#include "fontdata.h"
#include "rectangle.h"
#include "settings.h"
#include "utils.h"
#include "widget.h"
#include "xconnection.h"

enum {
    TopBorder = 0,
    RightBorder = 1,
    BottomBorder = 2,
    LeftBorder = 3,
};

using std::pair;
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
    if (style.display == CssDisplay::none) {
        return;
    }
    Rectangle geo =
            widget
            .geometryCached()
            .shifted(pixmapPos_ * -1)
            .adjusted(-style.marginLeft,
                      -style.marginTop,
                      -style.marginRight,
                      -style.marginBottom);
    style.backgroundColor.ifRight([&](const Color& bgcol) {
        fillRectangle(geo, bgcol);
    });

    Rectangle outline = geo.adjusted(style.outlineWidthLeft, style.outlineWidthTop,
                                     style.outlineWidthRight, style.outlineWidthBottom);
    int outlineWidth[4] = {
        style.outlineWidthTop,
        style.outlineWidthRight,
        style.outlineWidthBottom,
        style.outlineWidthLeft,
    };
    Color outlineColor[4] = {
        style.outlineColorTop,
        style.outlineColorRight,
        style.outlineColorBottom,
        style.outlineColorLeft,
    };
    drawBorder(outline, outlineWidth, outlineColor);

    int borderWidth[4] = {
        style.borderWidthTop,
        style.borderWidthRight,
        style.borderWidthBottom,
        style.borderWidthLeft,
    };
    Color borderColor[4] = {
        style.borderColorTop,
        style.borderColorRight,
        style.borderColorBottom,
        style.borderColorLeft,
    };
    drawBorder(geo, borderWidth, borderColor);

    for (const Widget* child : widget.nestedWidgets_) {
        render(*child);
    }
    if (widget.textContent_) {
        Rectangle contentGeo = widget.contentGeometryCached();
        Point2D textPos = contentGeo.tl();
        HSFont font = style.font.cases<HSFont>(
        [](const Unit<BoxStyle::initial>& u) {
            return HSFont::defaultFont();
        }, [](const HSFont& f) {
            return f;
        });
        int textHeight = style.textHeight.rightOr(font.data().ascent);
        int textDepth = style.textDepth.rightOr(font.data().descent);
        int extraSpace = contentGeo.height - textHeight - textDepth;
        textPos.y += extraSpace / 2 + textHeight;
        if (textHeight != 0) {
            drawText(pixmap_, gc_, font.data(), style.fontColor,
                     textPos - pixmapPos_, widget.textContent_(), contentGeo.width, style.textAlign);
        }
    }
}

void X11WidgetRender::drawBorder(Rectangle outer, int width[4], Color color[4])
{
    Rectangle inner = outer.adjusted(-width[LeftBorder],
                                     -width[TopBorder],
                                     -width[RightBorder],
                                     -width[BottomBorder]);
    // first draw the rectangular subset of the borders:
    vector<pair<const Color*,Rectangle>> borderRects = {
        {color + TopBorder,
         {inner.x, outer.y, inner.width, width[TopBorder]}},
        {color + RightBorder,
         {inner.x + inner.width, inner.y, width[RightBorder], inner.height}},
        {color + BottomBorder,
         {inner.x, inner.y + inner.height, inner.width, width[BottomBorder]}},
        {color + LeftBorder,
         {outer.x, inner.y, width[LeftBorder], inner.height}},
    };
    for (const auto& p : borderRects) {
        if (p.second) {
            fillRectangle(p.second, *(p.first));
        }
    }
    // top left corner
    Rectangle tlcorn = {
        outer.x, outer.y, width[LeftBorder], width[TopBorder]
    };
    fillTriangle(tlcorn.tl(), tlcorn.bl(), tlcorn.br(), color[LeftBorder]);
    fillTriangle(tlcorn.tl(), tlcorn.tr(), tlcorn.br(), color[TopBorder]);
    // top right corner
    Rectangle trcorn = {
        inner.x + inner.width,
        outer.y, width[RightBorder], width[TopBorder]
    };
    fillTriangle(trcorn.tl(), trcorn.tr(), trcorn.bl(), color[TopBorder]);
    fillTriangle(trcorn.tr(), trcorn.br(), trcorn.bl(), color[RightBorder]);
    // bottom right corner
    Rectangle brcorn = {
        inner.x + inner.width,
        inner.y + inner.height,
        width[RightBorder], width[BottomBorder]
    };
    fillTriangle(brcorn.tl(), brcorn.tr(), brcorn.br(), color[RightBorder]);
    fillTriangle(brcorn.br(), brcorn.bl(), brcorn.tl(), color[BottomBorder]);
    // bottom left corner
    Rectangle blcorn = {
        outer.x, inner.y + inner.height,
        width[LeftBorder], width[BottomBorder]
    };
    fillTriangle(blcorn.tr(), blcorn.br(), blcorn.bl(), color[BottomBorder]);
    fillTriangle(blcorn.tl(), blcorn.tr(), blcorn.bl(), color[LeftBorder]);
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

void X11WidgetRender::fillTriangle(Point2D p1, Point2D p2, Point2D p3, const Color& color)
{
    XSetForeground(xcon_.display(),
                   gc_,
                   xcon_.allocColor(colormap_, color));
    XPoint corners[3] = {
        p1.toXPoint(),
        p2.toXPoint(),
        p3.toXPoint(),
    };
    XFillPolygon(xcon_.display(), pixmap_, gc_,
                 corners, 3, Convex, CoordModeOrigin);
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
