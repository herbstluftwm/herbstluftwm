#include "decoration.h"

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <limits>

#include "client.h"
#include "ewmh.h"
#include "font.h"
#include "fontdata.h"
#include "root.h"
#include "settings.h"
#include "theme.h"
#include "xconnection.h"

using std::string;
using std::vector;

std::map<Window,Client*> Decoration::decwin2client;

// from openbox/frame.c
Visual* Decoration::check_32bit_client(Client* c)
{
    XConnection& xcon = xconnection();
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
    if (visual) {
        /* client has a 32-bit visual */
        mask = CWColormap | CWBackPixel | CWBorderPixel;
        /* create a colormap with the visual */
        dec->colormap = at.colormap =
            XCreateColormap(display, xcon.root(), visual, AllocNone);
        at.background_pixel = BlackPixel(display, xcon.screen());
        at.border_pixel = BlackPixel(display, xcon.screen());
    } else {
        dec->colormap = 0;
    }
    dec->depth = visual
                 ? 32
                 : (DefaultDepth(display, xcon.screen()));
    dec->decwin = XCreateWindow(display, xcon.root(), 0,0, 30, 30, 0,
                        dec->depth,
                        InputOutput,
                        visual
                            ? visual
                            : DefaultVisual(display, xcon.screen()),
                        mask, &at);
    mask = 0;
    if (visual) {
        /* client has a 32-bit visual */
        mask = CWColormap | CWBackPixel | CWBorderPixel;
        // TODO: why does DefaultColormap work in openbox but crashes hlwm here?
        // It somehow must be incompatible to the visual and thus causes the
        // BadMatch on XCreateWindow
        at.colormap = dec->colormap;
        at.background_pixel = BlackPixel(display, xcon.screen());
        at.border_pixel = BlackPixel(display, xcon.screen());
    }
    dec->bgwin = 0;
    dec->bgwin = XCreateWindow(display, dec->decwin, 0,0, 30, 30, 0,
                        dec->depth,
                        InputOutput,
                        CopyFromParent,
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
    resize_outline(scheme.inner_rect_to_outline(inner), scheme);
    last_rect_inner = true;
}

Rectangle Decoration::inner_to_outer(Rectangle rect) {
    if (!last_scheme) {
        // if the decoration was never drawn, we can't do anything reasonable
        return rect;
    }
    return last_scheme->inner_rect_to_outline(rect);
}

/**
 * @brief Tell whether clicking on the decoration at the specified location
 * should result in resizing or moving the client
 * @param the location of the cursor, relative on this window
 * @return true if this should init resizing the client
 * false if this should init moving the client
 */
bool Decoration::positionTriggersResize(Point2D p)
{
    if (!last_scheme) {
        // this should never happen, so we just randomly pick:
        // always resize if there is no decoration scheme
        return true;
    }
    const
    auto border_width = static_cast<int>(last_scheme->border_width());
    vector<Point2D> corners = {
        {0,0},
        {last_outer_rect.width - 1, last_outer_rect.height - 1},
    };
    for (const auto& c : corners) {
        if (std::abs(p.x - c.x) < border_width) {
            return true;
        }
        if (std::abs(p.y - c.y) < border_width) {
            return true;
        }
    }
    return false;
}

void Decoration::resize_outline(Rectangle outline, const DecorationScheme& scheme)
{
    auto inner = scheme.outline_to_inner_rect(outline);
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

    if (scheme.tight_decoration()) {
        // updating the outline only has an affect for tiled clients
        // because for floating clients, this has been done already
        // right when the window size changed.
        outline = scheme.inner_rect_to_outline(inner);
    }
    last_inner_rect = inner;
    inner.x -= outline.x;
    inner.y -= outline.y;
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
    redrawPixmap();
    XConnection& xcon = xconnection();
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
    XMoveResizeWindow(xcon.display(), decwin,
                      outline.x, outline.y, outline.width, outline.height);
    updateFrameExtends();
    if (!client_->dragged_ || settings_.update_dragged_clients()) {
        client_->send_configure();
    }
    XSync(xcon.display(), False);
}

void Decoration::updateFrameExtends() {
    int left = last_inner_rect.x - last_outer_rect.x;
    int top  = last_inner_rect.y - last_outer_rect.y;
    int right = last_outer_rect.width - last_inner_rect.width - left;
    int bottom = last_outer_rect.height - last_inner_rect.height - top;
    client_->ewmh.updateFrameExtents(client_->window_, left,right, top,bottom);
}

XConnection& Decoration::xconnection()
{
    return Root::get()->X;
}

void Decoration::change_scheme(const DecorationScheme& scheme) {
    if (last_inner_rect.width < 0) {
        // TODO: do something useful here
        return;
    }
    if (last_rect_inner) {
        resize_inner(last_inner_rect, scheme);
    } else {
        resize_outline(last_outer_rect, scheme);
    }
}

void Decoration::redraw()
{
    if (last_scheme) {
        change_scheme(*last_scheme);
    }
}

unsigned long Decoration::get_client_color(Color color) {
    XConnection& xcon = xconnection();
    XColor xcol = color.toXColor();
    if (colormap) {
        /* get pixel value back appropriate for client */
        XAllocColor(xcon.display(), colormap, &xcol);
        // explicitly set the alpha-byte to 0xff (fully opaque)
        return xcol.pixel | (0xffu << 24);
    } else {
        /* get pixel value back appropriate for main color map*/
        XAllocColor(xcon.display(), DefaultColormap(xcon.display(), xcon.screen()), &xcol);
        // explicitly set the alpha-byte to 0xff (fully opaque)
        return xcol.pixel | (0xffu << 24);
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
        FontData& fontData = s.title_font->data();
        string title = client_->title_();
        Point2D titlepos = {
            static_cast<int>(s.padding_left() + s.border_width()),
            static_cast<int>(s.title_height())
        };
        if (fontData.xftFont_) {
            Visual* xftvisual = visual ? visual : DefaultVisual(display, xcon.screen());
            Colormap xftcmap = colormap ? colormap : DefaultColormap(display, xcon.screen());
            XftDraw* xftd = XftDrawCreate(display, pix, xftvisual, xftcmap);
            XRenderColor xrendercol = {
                    s.title_color->red_,
                    s.title_color->green_,
                    s.title_color->blue_,
                    0xffff, // alpha as set by XftColorAllocName()
            };
            XftColor xftcol = { };
            XftColorAllocValue(display, xftvisual, xftcmap, &xrendercol, &xftcol);
            XftDrawStringUtf8(xftd, &xftcol, fontData.xftFont_,
                           titlepos.x, titlepos.y,
                           (const XftChar8*)title.c_str(), title.size());
            XftDrawDestroy(xftd);
            XftColorFree(display, xftvisual, xftcmap, &xftcol);
        } else if (fontData.xFontSet_) {
            XSetForeground(display, gc, get_client_color(s.title_color));
            XmbDrawString(display, pix, fontData.xFontSet_, gc, titlepos.x, titlepos.y,
                    title.c_str(), title.size());
        } else if (fontData.xFontStruct_) {
            XSetForeground(display, gc, get_client_color(s.title_color));
            XFontStruct* font = s.title_font->data().xFontStruct_;
            XSetFont(display, gc, font->fid);
            XDrawString(display, pix, gc, titlepos.x, titlepos.y,
                    title.c_str(), title.size());
        }
    }
    // clean up
    XFreeGC(display, gc);
}

