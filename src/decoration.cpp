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
#include "utils.h"
#include "xconnection.h"
#include "x11-widgetrender.h"

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

Decoration::Decoration(Client* client, Settings& settings, Theme& theme)
    : client_(client)
    , settings_(settings)
    , theme_(theme)
{
    widMain.vertical_ = true;
    widMain.addChild(&widTabBar);
    widMain.addChild(&widClient);
    widTabBar.setClassEnabled(CssName::Builtin::tabbar, true);
    widClient.expandX_ = true;
    widClient.expandY_ = true;
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
                        visual ? visual : xcon.visual(),
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
                            0, InputOnly, visual ? visual : xcon.visual(),
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
    widTabBar.clearChildren();
    for (TabWidget* w : widTabs) {
        delete w;
    }
    widTabs.clear();
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

void Decoration::setParameters(const DecorationParameters& params)
{
    // make sure the number of tab widgets is correct:
    if (params.tabs_.size() != widTabs.size()) {
        if (params.tabs_.size() > widTabs.size()) {
            // we need more tabs
            widTabs.reserve(params.tabs_.size());
            while (params.tabs_.size() > widTabs.size()) {
                TabWidget* newTab = new TabWidget();
                newTab->minimumSizeUser_ = {10,10};
                widTabs.push_back(newTab);
                widTabBar.addChild(newTab);
            }
        } else {
            // we need fewer tabs:
            auto firstToRemove = widTabs.begin() + params.tabs_.size();
            // remove all from tab bar and add remaining widgets again:
            widTabBar.clearChildren();
            for (auto it = widTabs.begin(); it != firstToRemove; it++) {
                widTabBar.addChild(*it);
            }
            // actually free the memory:
            for (auto it = firstToRemove; it != widTabs.end(); it++) {
                TabWidget* t = *it;
                delete t;
            }
            widTabs.erase(firstToRemove, widTabs.end());
        }
    }
    // now the vector sizes match, so we can sync the contents:
    for (size_t i = 0; i < params.tabs_.size(); i++) {
        widTabs[i]->tabClient = params.tabs_[i];
        widTabs[i]->setClassEnabled({
            {CssName::Builtin::focus, params.tabs_[i] == client_},
            {CssName::Builtin::urgent, params.tabs_[i] == client_},
        });
    }
    // set the css classes
    CssNameSet classes;
    classes.setEnabled({
       {{CssName::Builtin::window}, true},
       {{CssName::Builtin::focus}, params.focused_},
    });
    widMain.setClasses(classes);

    // and compute the resulting styles
    widMain.recurse([this](Widget& wid) {
        wid.setStyle(this->theme_.computeBoxStyle(&wid));
    });
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

void Decoration::resize_inner(Rectangle inner) {
    // we need to update (i.e. clear) tabs before inner_rect_to_outline()
    if (client_->decorated_()) {
        client_->applysizehints(&inner.width, &inner.height);
        widClient.minimumSizeUser_ = inner.dimensions();
        widMain.computeMinimumSize();
        Point2D outerSize = widMain.minimumSizeCached();
        widMain.computeGeometry({0, 0, outerSize.x, outerSize.y});
        // move everything such that widClient.tl() is inner.tl():
        widMain.moveGeometryCached(inner.tl() - widClient.geometryCached().tl());
    }
    last_rect_inner = true;
    applyWidgetGeometries();
}

Rectangle Decoration::inner_to_outer(Rectangle rect) {
    if (!last_scheme) {
        // if the decoration was never drawn, just take a guess.
        // Since the 'inner' rect is usually a floating geometry,
        // take a scheme from there.
        const DecorationScheme& fallback =  client_->theme.floating.normal;
        return fallback.inner_rect_to_outline(rect, tabs_.size());
    }
    return last_scheme->inner_rect_to_outline(rect, tabs_.size());
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

/**
 * @brief ensure that the other mentioned client is removed
 * from the tab bar of 'this' client.
 * @param otherClientTab
 */
void Decoration::removeFromTabBar(Client* otherClientTab)
{
    tabs_.erase(std::remove_if(tabs_.begin(), tabs_.end(),
                               [=](Client* c) {
        return c == otherClientTab;
    }), tabs_.end());
}

void Decoration::resize_outline(Rectangle outline)
{
    Rectangle inner;
    widClient.minimumSizeUser_ = {WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT};
    widMain.computeMinimumSize();

    widMain.computeGeometry(outline);
    last_rect_inner = false;
    applyWidgetGeometries();
}

void Decoration::applyWidgetGeometries() {
    bool decorated = client_->decorated_();
    Window win = client_->window_;
    const auto tile = widClient.geometryCached();
    Rectangle inner =
            decorated
            ? widClient.geometryCached()
            : widMain.geometryCached();
    client_->applysizehints(&inner.width, &inner.height);

    // center the window in the outline tile
    // but only if it's relative coordinates would not be too close to the
    // upper left tile border
    int threshold = settings_.pseudotile_center_threshold;
    int dx = tile.width/2 - inner.width/2;
    int dy = tile.height/2 - inner.height/2;
    inner.x = tile.x + ((dx < threshold) ? 0 : dx);
    inner.y = tile.y + ((dy < threshold) ? 0 : dy);

    last_inner_rect = inner;
    if (decorated) {
        // if the window is decorated, then x/y are relative
        // to the decoration window's top left
        inner.x -= widMain.geometryCached().x;
        inner.y -= widMain.geometryCached().y;
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
    bool size_changed = widMain.geometryCached().dimensions() != last_outer_rect.dimensions();
    last_outer_rect = widMain.geometryCached();
    last_rect_inner = false;
    client_->last_size_ = inner;
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
        Rectangle outline = widMain.geometryCached();
        Point2D borderWidth =  outline.br() - widClient.geometryCached().br();
        int bw = std::max(borderWidth.x, borderWidth.y);
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
    int left = 0;
    int top  = 0;
    int right = 0;
    int bottom = 0;
    if (client_->decorated_()) {
        Point2D tl = widClient.geometryCached().tl() - widMain.geometryCached().tl();
        Point2D br = widMain.geometryCached().br() - widClient.geometryCached().br();
        left = tl.x;
        top = tl.y;
        right = br.x;
        bottom = br.y;
    }
    client_->ewmh.updateFrameExtents(client_->window_, left,right, top,bottom);
}

XConnection& Decoration::xconnection()
{
    return XConnection::get();
}

void Decoration::redraw()
{
    if (client_->decorated_()) {
        applyWidgetGeometries();
    }
}

// draw a decoration to the client->dec.pixmap
void Decoration::redrawPixmap() {
    XConnection& xcon = xconnection();
    Display* display = xcon.display();
    auto dec = this;
    auto outer = widMain.geometryCached();
    // TODO: maybe do something like pixmap recreate threshhold?
    bool recreate_pixmap = (dec->pixmap == 0) || (dec->pixmap_width != outer.width)
                                              || (dec->pixmap_height != outer.height);
    if (recreate_pixmap) {
        if (dec->pixmap) {
            XFreePixmap(display, dec->pixmap);
        }
        dec->pixmap = XCreatePixmap(display, decwin,
                                    static_cast<unsigned int>(outer.width),
                                    static_cast<unsigned int>(outer.height),
                                    depth);
    }
    buttons_.clear();
    Pixmap pix = dec->pixmap;
    GC gc = XCreateGC(display, pix, 0, nullptr);

    X11WidgetRender painter(pix, outer.tl(), colormap, gc);
    painter.render(widMain);
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
 * @param the horizontal alignment within this maximum width
 * @param text
 */
void Decoration::drawText(Pixmap& pix, GC& gc, const FontData& fontData, const Color& color,
                          Point2D position, const string& text, int width,
                          const TextAlign& align)
{
    XConnection& xcon = xconnection();
    Display* display = xcon.display();
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
                       (const XftChar8*)final_c_str, textLen);
        XftDrawDestroy(xftd);
        XftColorFree(display, xftvisual, xftcmap, &xftcol);
    } else if (fontData.xFontSet_) {
        XSetForeground(display, gc, xcon.allocColor(colormap, color));
        XmbDrawString(display, pix, fontData.xFontSet_, gc, position.x, position.y,
                final_c_str, textLen);
    } else if (fontData.xFontStruct_) {
        XSetForeground(display, gc, xcon.allocColor(colormap, color));
        XFontStruct* font = fontData.xFontStruct_;
        XSetFont(display, gc, font->fid);
        XDrawString(display, pix, gc, position.x, position.y,
                final_c_str, textLen);
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

TabWidget::TabWidget()
{
    expandX_ = true;
    CssNameSet classes;
    classes.setEnabled({
        { CssName::Builtin::tab, true },
    });
    setClasses(classes);
}
