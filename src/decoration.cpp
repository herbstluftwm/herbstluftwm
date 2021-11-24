#include "decoration.h"

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <algorithm>
#include <limits>
#include <vector>

#include "client.h"
#include "ewmh.h"
#include "font.h"
#include "fontdata.h"
#include "settings.h"
#include "theme.h"
#include "xconnection.h"

using std::string;
using std::swap;
using std::vector;
using std::pair;

std::map<Window,Client*> Decoration::decwin2client;

// from openbox/frame.c
Visual* Decoration::check_32bit_client(Client* c)
{
    XConnection& xcon = xconnection();
    if (xcon.usesTransparency()) {
        // if we already use transparency everywhere
        // then we do not need to handle transparent clients explicitly.
        return nullptr;
    }
    XWindowAttributes wattrib;
    Status ret;

    ret = XGetWindowAttributes(xcon.display(), c->window_, &wattrib);
    HSWeakAssert(ret != BadDrawable);
    HSWeakAssert(ret != BadWindow);

    if (wattrib.depth == 32) {
        return wattrib.visual;
    }
    return nullptr;
}

Decoration::Decoration(Client* client, Settings& settings)
    : client_(client),
      settings_(settings)
{
}

void Decoration::createWindow() {
    Decoration* dec = this;
    XConnection& xcon = xconnection();
    Display* display = xcon.display();
    XSetWindowAttributes at;
    long mask = 0;
    // copy attributes from client and not from the root window
    visual = check_32bit_client(client_);
    if (visual || xcon.usesTransparency()) {
        /* client has a 32-bit visual */
        mask = CWColormap | CWBackPixel | CWBorderPixel;
        if (visual) {
            // if the client has a visual different to the one of xconnection,
            // then create a colormap with the visual
            dec->colormap = XCreateColormap(display, xcon.root(), visual, AllocNone);
            at.colormap = dec->colormap;
        } else {
            at.colormap = xcon.colormap();
        }
        at.background_pixel = BlackPixel(display, xcon.screen());
        at.border_pixel = BlackPixel(display, xcon.screen());
    } else {
        dec->colormap = 0;
    }
    dec->depth = visual
                 ? 32
                 : xcon.depth();
    dec->decwin = XCreateWindow(display, xcon.root(), 0,0, 30, 30, 0,
                        dec->depth,
                        InputOutput,
                        visual
                            ? visual
                            : xcon.visual(),
                        mask, &at);
    mask = 0;
    if (visual || xcon.usesTransparency()) {
        /* client has a 32-bit visual */
        mask = CWColormap | CWBackPixel | CWBorderPixel;
        // TODO: why does DefaultColormap work in openbox but crashes hlwm here?
        // It somehow must be incompatible to the visual and thus causes the
        // BadMatch on XCreateWindow
        at.colormap = xcon.usesTransparency() ? xcon.colormap() : dec->colormap;
        at.background_pixel = BlackPixel(display, xcon.screen());
        at.border_pixel = BlackPixel(display, xcon.screen());
    }
    dec->bgwin = 0;
    dec->bgwin = XCreateWindow(display, dec->decwin, 0,0, 30, 30, 0,
                        dec->depth,
                        InputOutput,
                        xcon.visual(),
                        mask, &at);
    XMapWindow(display, dec->bgwin);
    // use a clients requested initial floating size as the initial size
    dec->last_rect_inner = true;
    dec->last_inner_rect = client_->float_size_;
    dec->last_outer_rect = client_->float_size_; // TODO: is this correct?
    dec->last_actual_rect = dec->last_inner_rect;
    dec->last_actual_rect.x -= dec->last_outer_rect.x;
    dec->last_actual_rect.y -= dec->last_outer_rect.y;
    decwin2client[decwin] = client_;

    XSetWindowAttributes resizeAttr;
    resizeAttr.event_mask = 0; // we don't want any events such that the decoration window
                               // gets the events with the subwindow set to the respective
                               // resizeArea window
    resizeAttr.colormap = xcon.usesTransparency() ? xcon.colormap() : dec->colormap;
    resizeAttr.background_pixel = BlackPixel(display, xcon.screen());
    resizeAttr.border_pixel = BlackPixel(display, xcon.screen());
    for (size_t i = 0; i < resizeAreaSize; i++) {
        Window& win = resizeArea[i];
        win = XCreateWindow(display, dec->decwin, 0, 0, 30, 30, 0,
                            0, InputOnly, xcon.visual(),
                            CWEventMask, &resizeAttr);
        XMapWindow(display, win);
    }
    // set wm_class for window
    XClassHint *hint = XAllocClassHint();
    hint->res_name = (char*)HERBST_DECORATION_CLASS;
    hint->res_class = (char*)HERBST_DECORATION_CLASS;
    XSetClassHint(display, dec->decwin, hint);
    XFree(hint);
}

Decoration::~Decoration() {
    XConnection& xcon = xconnection();
    decwin2client.erase(decwin);
    if (colormap) {
        XFreeColormap(xcon.display(), colormap);
    }
    if (pixmap) {
        XFreePixmap(xcon.display(), pixmap);
    }
    if (bgwin) {
        XDestroyWindow(xcon.display(), bgwin);
    }
    if (decwin) {
        XDestroyWindow(xcon.display(), decwin);
    }
}

Client* Decoration::toClient(Window decoration_window)
{
    auto cl = decwin2client.find(decoration_window);
    if (cl == decwin2client.end()) {
        return nullptr;
    } else {
        return cl->second;
    }
}

Rectangle DecorationScheme::outline_to_inner_rect(Rectangle rect) const {
    return rect.adjusted(-*border_width, -*border_width)
            .adjusted(-*padding_left, -*padding_top - *title_height,
                      -*padding_right, -*padding_bottom);
}

Rectangle DecorationScheme::inner_rect_to_outline(Rectangle rect) const {
    return rect.adjusted(*border_width, *border_width)
            .adjusted(*padding_left, *padding_top + *title_height,
                      *padding_right, *padding_bottom);
}

void Decoration::resize_inner(Rectangle inner, const DecorationScheme& scheme) {
    // if the client is undecorated, the outline is identical to the inner geometry
    // otherwise, we convert the geometry using the theme
    auto outline = (client_->decorated_())
                   ? scheme.inner_rect_to_outline(inner)
                   : inner;
    resize_outline(outline, scheme, {});
    last_rect_inner = true;
}

Rectangle Decoration::inner_to_outer(Rectangle rect) {
    if (!last_scheme) {
        // if the decoration was never drawn, just take a guess.
        // Since the 'inner' rect is usually a floating geometry,
        // take a scheme from there.
        const DecorationScheme& fallback =  client_->theme.floating.normal;
        return fallback.inner_rect_to_outline(rect);
    }
    return last_scheme->inner_rect_to_outline(rect);
}

void Decoration::updateResizeAreaCursors()
{
    XConnection& xcon = xconnection();
    for (size_t i = 0; i < resizeAreaSize; i++) {
        Window& win = resizeArea[i];
        ResizeAction act = resizeAreaInfo(i);
        act = act * client_->possibleResizeActions();
        auto cursor = act.toCursorShape();
        if (cursor.has_value()) {
            XDefineCursor(xcon.display(), win, XCreateFontCursor(xcon.display(), cursor.value()));
        } else {
            XUndefineCursor(xcon.display(), win);
        }
    }
}

std::experimental::optional<Decoration::ClickArea>
Decoration::positionHasButton(Point2D p)
{
    for (auto& button : buttons_) {
        if (button.area_.contains(p)) {
            return button;
        }
    }
    return {};
}

/**
 * @brief Tell whether clicking on the decoration at the specified location
 * should result in resizing or moving the client
 * @param the location of the cursor, relative on this window
 * @return Flags indicating the decoration borders that should be resized
 */
ResizeAction Decoration::positionTriggersResize(Point2D p)
{
    if (!last_scheme) {
        // this should never happen, so we just randomly pick:
        // never resize if there is no decoration scheme
        return ResizeAction();
    }
    auto border_width = static_cast<int>(last_scheme->border_width());
    ResizeAction act;
    if (p.x < border_width) {
        act.left = True;
    }
    if (p.x + border_width >= last_outer_rect.width) {
        act.right = True;
    }
    if (act.left || act.right) {
        if (p.y < last_outer_rect.height / 3) {
            act.top = True;
        } else if (p.y > (2 * last_outer_rect.height) / 3) {
            act.bottom = True;
        }
    }
    if (p.y < border_width) {
        act.top = True;
    }
    if (p.y + border_width >= last_outer_rect.height) {
        act.bottom = True;
    }
    if (act.top || act.bottom) {
        if (p.x < last_outer_rect.width / 3) {
            act.left = True;
        } else if (p.x > (2 * last_outer_rect.width) / 3) {
            act.right = True;
        }
    }
    return act;
}

/**
 * @brief Find the most appropriate ResizeAction given the current
 * cursor position. This is a very fuzzy version of positionTriggersResize()
 * @param the cursor position
 * @return the suggested return action
 */
ResizeAction Decoration::resizeFromRoughCursorPosition(Point2D cursor)
{
    if (!last_scheme) {
        // this should never happen, so we just randomly pick:
        // never resize if there is no decoration scheme
        return ResizeAction();
    }
    Point2D cursorRelativeToCenter =
            cursor - last_outer_rect.tl() - last_outer_rect.dimensions() / 2;
    ResizeAction ra;
    ra.left = cursorRelativeToCenter.x < 0;
    ra.right = !ra.left;
    ra.top = cursorRelativeToCenter.y < 0;
    ra.bottom = !ra.top;
    return ra;
}

void Decoration::resize_outline(Rectangle outline, const DecorationScheme& scheme, vector<Client*> tabs)
{
    bool decorated = client_->decorated_();
    auto inner = scheme.outline_to_inner_rect(outline);
    if (!decorated) {
        inner = outline;
    }
    Window win = client_->window_;

    auto tile = inner;
    client_->applysizehints(&inner.width, &inner.height);

    // center the window in the outline tile
    // but only if it's relative coordinates would not be too close to the
    // upper left tile border
    int threshold = settings_.pseudotile_center_threshold;
    int dx = tile.width/2 - inner.width/2;
    int dy = tile.height/2 - inner.height/2;
    inner.x = tile.x + ((dx < threshold) ? 0 : dx);
    inner.y = tile.y + ((dy < threshold) ? 0 : dy);

    //if (RECTANGLE_EQUALS(client->last_size, rect)
    //    && client->last_border_width == border_width) {
    //    return;
    //}

    if (decorated && scheme.tight_decoration()) {
        // updating the outline only has an affect for tiled clients
        // because for floating clients, this has been done already
        // right when the window size changed.
        outline = scheme.inner_rect_to_outline(inner);
    }
    last_inner_rect = inner;
    if (decorated) {
        // if the window is decorated, then x/y are relative
        // to the decoration window's top left
        inner.x -= outline.x;
        inner.y -= outline.y;
    }
    XWindowChanges changes;
    changes.x = inner.x;
    changes.y = inner.y;
    changes.width = inner.width;
    changes.height = inner.height;
    changes.border_width = 0;

    int mask = CWX | CWY | CWWidth | CWHeight | CWBorderWidth;
    //if (*g_window_border_inner_width > 0
    //    && *g_window_border_inner_width < *g_window_border_width) {
    //    unsigned long current_border_color = get_window_border_color(client);
    //    HSDebug("client_resize %s\n",
    //            current_border_color == g_window_border_active_color
    //            ? "ACTIVE" : "NORMAL");
    //    set_window_double_border(g_display, win, *g_window_border_inner_width,
    //                             g_window_border_inner_color,
    //                             current_border_color);
    //}
    // send new size to client
    // update structs
    bool size_changed = outline.width != last_outer_rect.width
                     || outline.height != last_outer_rect.height;
    last_outer_rect = outline;
    last_rect_inner = false;
    tabs_ = tabs;
    client_->last_size_ = inner;
    last_scheme = &scheme;
    // redraw
    // TODO: reduce flickering
    if (!client_->dragged_ || settings_.update_dragged_clients()) {
        last_actual_rect.x = changes.x;
        last_actual_rect.y = changes.y;
        last_actual_rect.width = changes.width;
        last_actual_rect.height = changes.height;
    }
    XConnection& xcon = xconnection();
    if (decorated) {
        redrawPixmap();
        XSetWindowBackgroundPixmap(xcon.display(), decwin, pixmap);
        if (!size_changed) {
            // if size changes, then the window is cleared automatically
            XClearWindow(xcon.display(), decwin);
        }
        if (!client_->dragged_ || settings_.update_dragged_clients()) {
            XConfigureWindow(xcon.display(), win, mask, &changes);
            XMoveResizeWindow(xcon.display(), bgwin,
                              changes.x, changes.y,
                              changes.width, changes.height);
        }
    } else {
        // resize the client window
        XConfigureWindow(xcon.display(), win, mask, &changes);
    }
    // update geometry of resizeArea window
    if (decorated) {
        int bw = 0;
        if (last_scheme) {
            bw = last_scheme->border_width();
        }
        Rectangle areaGeo;
        for (size_t i = 0; i < resizeAreaSize; i++) {
            areaGeo = resizeAreaGeometry(i, bw, outline.width, outline.height);
            XMoveResizeWindow(xcon.display(), resizeArea[i],
                              areaGeo.x, areaGeo.y,
                              areaGeo.width, areaGeo.height);
        }
        XMoveResizeWindow(xcon.display(), decwin,
                          outline.x, outline.y, outline.width, outline.height);
    }
    updateFrameExtends();
    if (!client_->dragged_ || settings_.update_dragged_clients()) {
        client_->send_configure(false);
    }
    XSync(xcon.display(), False);
}

void Decoration::updateFrameExtends() {
    int left = last_inner_rect.x - last_outer_rect.x;
    int top  = last_inner_rect.y - last_outer_rect.y;
    int right = last_outer_rect.width - last_inner_rect.width - left;
    int bottom = last_outer_rect.height - last_inner_rect.height - top;
    if (!client_->decorated_()) {
        left = 0;
        top = 0;
        right = 0;
        bottom = 0;
    }
    client_->ewmh.updateFrameExtents(client_->window_, left,right, top,bottom);
}

XConnection& Decoration::xconnection()
{
    return XConnection::get();
}

void Decoration::change_scheme(const DecorationScheme& scheme) {
    if (last_inner_rect.width < 0) {
        // TODO: do something useful here
        return;
    }
    if (last_rect_inner) {
        resize_inner(last_inner_rect, scheme);
    } else {
        resize_outline(last_outer_rect, scheme, tabs_);
    }
}

void Decoration::redraw()
{
    if (client_->decorated_()) {
        if (last_scheme) {
            change_scheme(*last_scheme);
        }
    }
}

unsigned long Decoration::get_client_color(Color color) {
    XConnection& xcon = xconnection();
    XColor xcol = color.toXColor();
    if (colormap) {
        /* get pixel value back appropriate for client */
        XAllocColor(xcon.display(), colormap, &xcol);
        // explicitly set the alpha-byte to the one from the color
        return Color::x11PixelPlusAlpha(xcol.pixel, color.alpha_);
    } else {
        // the pixel value reported by Color already includes the
        // alpha value
        return color.toX11Pixel();
    }
}

// draw a decoration to the client->dec.pixmap
void Decoration::redrawPixmap() {
    if (!last_scheme) {
        // do nothing if we don't have a scheme.
        return;
    }
    XConnection& xcon = xconnection();
    Display* display = xcon.display();
    const DecorationScheme& s = *last_scheme;
    auto dec = this;
    auto outer = last_outer_rect;
    // TODO: maybe do something like pixmap recreate threshhold?
    bool recreate_pixmap = (dec->pixmap == 0) || (dec->pixmap_width != outer.width)
                                              || (dec->pixmap_height != outer.height);
    if (recreate_pixmap) {
        if (dec->pixmap) {
            XFreePixmap(display, dec->pixmap);
        }
        dec->pixmap = XCreatePixmap(display, decwin, outer.width, outer.height, depth);
    }
    buttons_.clear();
    Pixmap pix = dec->pixmap;
    GC gc = XCreateGC(display, pix, 0, nullptr);

    // draw background
    XSetForeground(display, gc, get_client_color(s.border_color()));
    XFillRectangle(display, pix, gc, 0, 0, outer.width, outer.height);

    // Draw inner border
    unsigned short iw = s.inner_width();
    auto inner = last_inner_rect;
    inner.x -= last_outer_rect.x;
    inner.y -= last_outer_rect.y;
    if (iw > 0) {
        /* fill rectangles because drawing does not work */
        vector<XRectangle> rects{
            { (short)(inner.x - iw), (short)(inner.y - iw), (unsigned short)(inner.width + 2*iw), iw }, /* top */
            { (short)(inner.x - iw), (short)(inner.y), iw, (unsigned short)(inner.height) },  /* left */
            { (short)(inner.x + inner.width), (short)(inner.y), iw, (unsigned short)(inner.height) }, /* right */
            { (short)(inner.x - iw), (short)(inner.y + inner.height), (unsigned short)(inner.width + 2*iw), iw }, /* bottom */
        };
        XSetForeground(display, gc, get_client_color(s.inner_color()));
        XFillRectangles(display, pix, gc, &rects.front(), rects.size());
    }

    // Draw outer border
    unsigned short ow = s.outer_width;
    outer.x -= last_outer_rect.x;
    outer.y -= last_outer_rect.y;
    if (ow > 0) {
        ow = std::min((int)ow, (outer.height+1) / 2);
        vector<XRectangle> rects{
            { 0, 0, (unsigned short)(outer.width), ow }, /* top */
            { 0, (short)ow, ow, (unsigned short)(outer.height - 2*ow) }, /* left */
            { (short)(outer.width - ow), (short)ow, ow, (unsigned short)(outer.height - 2*ow) }, /* right */
            { 0, (short)(outer.height - ow), (unsigned short)(outer.width), ow }, /* bottom */
        };
        XSetForeground(display, gc, get_client_color(s.outer_color));
        XFillRectangles(display, pix, gc, &rects.front(), rects.size());
    }
    // fill inner rect that is not covered by the client
    XSetForeground(display, gc, get_client_color(s.background_color));
    if (dec->last_actual_rect.width < inner.width) {
        XFillRectangle(display, pix, gc,
                       dec->last_actual_rect.x + dec->last_actual_rect.width,
                       dec->last_actual_rect.y,
                       inner.width - dec->last_actual_rect.width,
                       dec->last_actual_rect.height);
    }
    if (dec->last_actual_rect.height < inner.height) {
        XFillRectangle(display, pix, gc,
                       dec->last_actual_rect.x,
                       dec->last_actual_rect.y + dec->last_actual_rect.height,
                       inner.width,
                       inner.height - dec->last_actual_rect.height);
    }
    if (s.title_height() > 0) {
        Point2D titlepos = {
            static_cast<int>(s.padding_left() + s.border_width()),
            static_cast<int>(s.title_height())
        };
        if (tabs_.size() <= 1) {
            drawText(pix, gc, s.title_font->data(), s.title_color(),
                     titlepos, client_->title_(), inner.width);
        } else {
            int tabWidth = outer.width / tabs_.size();
            int tabIndex = 0;
            int tabPadLeft = inner.x;
            // if there is more than one tab
            for (Client* tabClient : tabs_) {
                bool isFirst = tabClient == tabs_.front();
                bool isLast = tabClient == tabs_.back();
                // we use the geometry from client_'s scheme,
                // but the colors from TabScheme
                const DecorationScheme& tabScheme =
                        (tabClient == client_)
                        ? s : tabClient->getDecorationScheme(false);
                Rectangle tabGeo {
                    tabIndex * tabWidth,
                    0,
                    tabWidth + int(isLast ? (outer.width % tabs_.size()) : 0),
                    int(s.title_height() + s.padding_top()), // tab height
                };
                if (tabClient != client_) {
                    // only add clickable buttons for the other clients
                    ClickArea tabButton;
                    tabButton.area_ = tabGeo;
                    tabButton.tabClient_ = tabClient;
                    buttons_.push_back(tabButton);
                }
                int titleWidth = tabGeo.width;
                if (tabClient == client_) {
                    tabGeo.height += s.border_width() - s.inner_width();
                }
                // tab background
                vector<XRectangle> fillRects = {
                    { (short)tabGeo.x, (short)tabGeo.y,
                      (unsigned short)tabGeo.width, (unsigned short)tabGeo.height },
                };
                XSetForeground(display, gc, get_client_color(tabScheme.border_color));
                XFillRectangles(display, pix, gc, &fillRects.front(), fillRects.size());
                vector<XRectangle> borderRects = {
                    // top edge
                    { (short)tabGeo.x, (short)tabGeo.y,
                      (unsigned short)tabGeo.width, (unsigned short)tabScheme.outer_width },
                };
                if (isFirst) {
                    // edge on the left
                    borderRects.push_back(
                    { (short)tabGeo.x, (short)tabGeo.y,
                      (unsigned short)tabScheme.outer_width, (unsigned short)tabGeo.height }
                    );
                } else if (client_ == tabClient) {
                    // shorter edge on the left
                    borderRects.push_back(
                    { (short)tabGeo.x, (short)tabGeo.y,
                      (unsigned short)tabScheme.outer_width,
                      (unsigned short)(tabGeo.height - (s.border_width() - s.outer_width() - s.inner_width())) }
                    );
                }
                if (isLast) {
                    // edge on the right
                    borderRects.push_back(
                    { (short)(tabGeo.x + tabGeo.width - tabScheme.outer_width), (short)tabGeo.y,
                      (unsigned short)tabScheme.outer_width,
                      (unsigned short)tabGeo.height }
                    );
                    titleWidth -= tabScheme.outer_width;
                } else if (client_ == tabClient) {
                    // shorter edge on the right
                    borderRects.push_back(
                    { (short)(tabGeo.x + tabGeo.width - tabScheme.outer_width), (short)tabGeo.y,
                      (unsigned short)tabScheme.outer_width,
                      (unsigned short)(tabGeo.height - (s.border_width() - s.outer_width() - s.inner_width())) }
                    );
                    titleWidth -= tabScheme.outer_width;
                }
                XSetForeground(display, gc, get_client_color(tabScheme.outer_color));
                XFillRectangles(display, pix, gc, &borderRects.front(), borderRects.size());
                if (client_ != tabClient) {
                    // horizontal border connecting the focused tab with the outer border
                    Point2D westEnd = tabGeo.bl();
                    XSetForeground(display, gc, get_client_color(s.outer_color));
                    XFillRectangle(display, pix, gc,
                                   westEnd.x, westEnd.y,
                                   tabGeo.width, s.outer_width()
                                   );
                    // horizontal border connecting the focused tab content with the outer border
                    int remainingBorderColorHeight = s.border_width() - s.inner_width() - s.outer_width();
                    int fillWidth = tabGeo.width;
                    if (isFirst || isLast) {
                        fillWidth -= s.outer_width();
                    }
                    if (isFirst) {
                        westEnd.x += s.outer_width();
                    }
                    XSetForeground(display, gc, get_client_color(s.border_color));
                    XFillRectangle(display, pix, gc,
                                   westEnd.x,
                                   westEnd.y + s.outer_width(),
                                   fillWidth, remainingBorderColorHeight
                                   );
                }
                drawText(pix, gc, tabScheme.title_font->data(), tabScheme.title_color(),
                         tabGeo.tl() + Point2D { tabPadLeft, (int)s.title_height()},
                         tabClient->title_(), titleWidth - tabPadLeft);
                tabIndex++;
            }
        }
    }
    // clean up
    XFreeGC(display, gc);
}

/**
 * @brief Draw a given text
 * @param pix The pixmap
 * @param gc The graphic context
 * @param fontData
 * @param color
 * @param position The position of the left end of the baseline
 * @param width The maximum width of the string (in pixels)
 * @param text
 */
void Decoration::drawText(Pixmap& pix, GC& gc, const FontData& fontData, const Color& color,
                          Point2D position, const string& text, int width)
{
    XConnection& xcon = xconnection();
    Display* display = xcon.display();
    // shorten the text first:
    size_t textLen = text.size();
    while (textLen > 0 && fontData.textwidth(text, textLen) > width) {
        textLen--;
    }
    if (fontData.xftFont_) {
        Visual* xftvisual = visual ? visual : xcon.visual();
        Colormap xftcmap = colormap ? colormap : xcon.colormap();
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
                       (const XftChar8*)text.c_str(), textLen);
        XftDrawDestroy(xftd);
        XftColorFree(display, xftvisual, xftcmap, &xftcol);
    } else if (fontData.xFontSet_) {
        XSetForeground(display, gc, get_client_color(color));
        XmbDrawString(display, pix, fontData.xFontSet_, gc, position.x, position.y,
                text.c_str(), textLen);
    } else if (fontData.xFontStruct_) {
        XSetForeground(display, gc, get_client_color(color));
        XFontStruct* font = fontData.xFontStruct_;
        XSetFont(display, gc, font->fid);
        XDrawString(display, pix, gc, position.x, position.y,
                text.c_str(), textLen);
    }
}

ResizeAction Decoration::resizeAreaInfo(size_t idx)
{
    /*
     *  first half: horizontal edges
     *  second half: vertical edges
     *  -0-1-2-
     * 6       9
     * 7      10
     * 8      11
     *  -3-4-5-
     */
    ResizeAction act;
    if (idx < 6) {
        // horizontal edge
        act.top = idx < 3;
        act.bottom = idx >= 3;
        act.left = (idx % 3) == 0;
        act.right = (idx % 3) == 2;
    } else {
        // vertical edge
        act.left = idx < 9;
        act.right = idx >= 9;
        act.top = (idx % 3) == 0;
        act.bottom = (idx % 3) == 2;
    }
    return act;
}

Rectangle Decoration::resizeAreaGeometry(size_t idx, int borderWidth, int width, int height)
{
    if (idx < 6) {
        if (borderWidth <= 0) {
            // ensure that the rectangles returned are not empty
            // i.e. that they have non-zero height and width
            borderWidth = 1;
        }
        int w3 = width / 3;
        Rectangle geo;
        // horizontal segments:
        geo.height = borderWidth;
        if (idx % 3 == 1) {
            // middle segment
            geo.width = width - 2 * w3;
            geo.x = w3;
        } else {
            geo.width = w3;
            if (idx % 3 == 0) {
                // left segment
                geo.x = 0;
            } else {
                // right segment
                geo.x = width - w3;
            }
        }
        if (idx < 3) {
            // upper border
            geo.y = 0;
        } else {
            geo.y = height - borderWidth;
        }
        return geo;
    } else {
        // vertical segments:
        // its the same as horizontal segments, only
        // with x- and y-dimensions swapped:
        Rectangle geo = resizeAreaGeometry(idx - 6, borderWidth, height, width);
        swap(geo.x, geo.y);
        swap(geo.width, geo.height);
        return geo;
    }
}


/**
 * @brief Return the x11 cursor shaper corresponding
 * to the ResizeAction or the default cursor shape
 * @return
 */
std::experimental::optional<unsigned int> ResizeAction::toCursorShape() const
{
    if (top) {
        if (left) {
            return XC_top_left_corner;
        } else if (right) {
            return XC_top_right_corner;
        } else {
            return XC_top_side;
        }
    } else if (bottom) {
        if (left) {
            return XC_bottom_left_corner;
        } else if (right) {
            return XC_bottom_right_corner;
        } else {
            return XC_bottom_side;
        }
    } else {
        if (left) {
            return XC_left_side;
        } else if (right) {
            return XC_right_side;
        } else {
            return {};
        }
    }
}
