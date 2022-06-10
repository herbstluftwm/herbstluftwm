#include "decoration.h"

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
    widMain.addChild(&widPanel);
    widPanel.addChild(&widTabBar);
    widTabBar.expandX_ = true;
    widPanel.setClassEnabled(CssName::Builtin::bar, true);
    widMain.addChild(&widClient);
    widTabBar.setClassEnabled(CssName::Builtin::tabbar, true);
    widClient.setClassEnabled(CssName::Builtin::client_content, true);
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
    if (lastParams == params) {
        return;
    }
    // make sure the number of tab widgets is correct:
    size_t tabsRequired = params.tabs_.size();
    if (tabsRequired == 0) {
        // we require at least one tab. if params has no tabs
        // then we create one tab holding the window title:
        tabsRequired = 1;
    }
    if (tabsRequired != widTabs.size()) {
        if (tabsRequired > widTabs.size()) {
            // we need more tabs
            widTabs.reserve(tabsRequired);
            while (tabsRequired > widTabs.size()) {
                TabWidget* newTab = new TabWidget();
                widTabs.push_back(newTab);
                widTabBar.addChild(newTab);
            }
        } else {
            // we need fewer tabs:
            auto firstToRemove = widTabs.begin() + tabsRequired;
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
    for (size_t i = 0; i < tabsRequired; i++) {
        // take the client from the parameters' tabs
        // or use our main client if tabsRequired was increased to 1
        Client* tabClient =
                (i < params.tabs_.size())
                ? params.tabs_[i]
                : client_;
        widTabs[i]->tabClient = tabClient;
        bool focus = tabClient == client_;
        widTabs[i]->setClassEnabled({
            {CssName::Builtin::focus, focus},
            {CssName::Builtin::urgent, tabClient->urgent_()},
            {CssName::Builtin::normal, !focus && !tabClient->urgent_()},
        });
    }
    // set the css classes
    CssNameSet classes;
    classes.setEnabled({
       {{CssName::Builtin::window}, true},
       {{CssName::Builtin::floating}, params.floating_},
       {{CssName::Builtin::tiling}, !params.floating_},
       {{CssName::Builtin::focus}, params.focused_},
       {{CssName::Builtin::urgent}, client_->urgent_()},
       {{CssName::Builtin::normal}, !params.focused_ && !client_->urgent_()},
       {{CssName::Builtin::minimal}, params.minimal_},
       {{CssName::Builtin::fullscreen}, params.fullscreen_},
       {{CssName::Builtin::no_tabs}, params.tabs_.size() == 0},
       {{CssName::Builtin::one_tab}, params.tabs_.size() == 1},
       {{CssName::Builtin::multiple_tabs}, params.tabs_.size() > 1},
    });
    widMain.setClasses(classes);

    // and compute the resulting styles
    widMain.recurse([this](Widget& wid) {
        wid.setStyle(this->theme_.computeBoxStyle(&wid));
    });
}

void Decoration::computeWidgetGeometries(Rectangle innerGeometry)
{
    widClient.minimumSizeUser_ = innerGeometry.dimensions();
    widMain.computeMinimumSize();
    Point2D outerSize = widMain.minimumSizeCached();
    widMain.computeGeometry({0, 0, outerSize.x, outerSize.y});
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
        widMain.moveGeometryCached(inner.tl() - widClient.contentGeometryCached().tl());
    }
    applyWidgetGeometries();
}

Rectangle Decoration::inner_to_outer(Rectangle rect) {
    Point2D deltaTL = last_outer().tl() - last_inner().tl();
    Point2D deltaBR = last_outer().br() - last_inner().br();
    return Rectangle::fromCorners(deltaTL + rect.tl(), deltaBR + rect.br());
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
    p = p + widMain.geometryCached().tl();
    for (TabWidget* tab : widTabs) {
        if (tab->geometryCached().contains(p) &&
            tab->tabClient != client_) {
            ClickArea button;
            button.area_ = tab->geometryCached();
            button.tabClient_ = tab->tabClient;
            return button;
        }
    }
    return {};
}

/*** estimate the border width from widget geometries
 */
int Decoration::borderWidth() const
{
    Point2D deltaTL = last_inner().tl() - last_outer().tl();
    Point2D deltaBR = last_inner().br() - last_outer().br();
    return
        std::max(
            std::min(std::abs(deltaTL.x), std::abs(deltaBR.x)),
            std::min(std::abs(deltaTL.y), std::abs(deltaBR.y)));
}

/**
 * @brief Tell whether clicking on the decoration at the specified location
 * should result in resizing or moving the client
 * @param the location of the cursor, relative on this window
 * @return Flags indicating the decoration borders that should be resized
 */
ResizeAction Decoration::positionTriggersResize(Point2D p)
{
    ResizeAction act;
    int border_width = borderWidth();
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
    for (size_t i = 0; i < widTabs.size(); i++) {
        if (widTabs[i]->tabClient == otherClientTab) {
            widTabBar.removeChild(i);
            delete widTabs[i];
            auto idxTyped = static_cast<vector<TabWidget*>::difference_type>(i);
            widTabs.erase(widTabs.begin() + idxTyped);
            return;
        }
    }
}

void Decoration::resize_outline(Rectangle outline)
{
    Rectangle inner;
    widClient.minimumSizeUser_ = {WINDOW_MIN_WIDTH, WINDOW_MIN_HEIGHT};
    widMain.computeMinimumSize();

    widMain.computeGeometry(outline);
    applyWidgetGeometries();
}

void Decoration::applyWidgetGeometries() {
    bool decorated = client_->decorated_();
    Window win = client_->window_;
    const auto tile = widClient.contentGeometryCached();
    Rectangle inner =
            decorated
            ? widClient.contentGeometryCached()
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
        Point2D borderWidth =  outline.br() - widClient.contentGeometryCached().br();
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
        Point2D tl = widClient.contentGeometryCached().tl() - widMain.geometryCached().tl();
        Point2D br = widMain.geometryCached().br() - widClient.contentGeometryCached().br();
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
    Pixmap pix = dec->pixmap;
    GC gc = XCreateGC(display, pix, 0, nullptr);

    X11WidgetRender painter(settings_, pix, outer.tl(), colormap, gc, visual);
    painter.render(widMain);
    // clean up
    XFreeGC(display, gc);
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
    hasText_ = true;
    setClasses(classes);
}

string TabWidget::textContent() const
{
    return tabClient->title_();
}

void DecorationParameters::updateTabUrgencyFlags()
{
    urgentTabs_.clear();
    for (Client* tab : tabs_) {
        urgentTabs_.push_back(tab->urgent_());
    }
}
