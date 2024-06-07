#include "x11-utils.h"

#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/shapeconst.h>

#include "globals.h"
#include "xconnection.h"

using std::vector;

/**
 * \brief cut the given rectangles out of the window using XShape
 */
void window_cut_rect_holes(XConnection& X, Window win, int width, int height,
                           const vector<Rectangle>& holes) {
    // inspired by the xhole.c example
    // http://www.answers.com/topic/xhole-c
    Display* d = X.display();
    GC gp;
    int bw = 100; // add a large border, just to be sure the border is visible
    width += 2*bw;
    height += 2*bw;

    /* create the pixmap that specifies the shape */
    Pixmap p = XCreatePixmap(d, win, width, height, 1);
    gp = XCreateGC(d, p, 0, nullptr);
    XSetForeground(d, gp, WhitePixel(d, X.screen()));
    XFillRectangle(d, p, gp, 0, 0, width, height);
    XSetForeground(d, gp, BlackPixel(d, X.screen()));
    //int radius = 50;
    //XFillArc(d, p, gp,
    //         width/2 - radius, height/2 - radius, radius, radius,
    //         0, 360*64);
    for (const auto& rect : holes) {
        XFillRectangle(d, p, gp, bw + rect.x, bw + rect.y,
                                 rect.width, rect.height);
    }

    /* set the pixmap as the new window mask;
    the pixmap is slightly larger than the window to allow for the window
    border and title bar (as added by the window manager) to be visible */
    XShapeCombineMask(d, win, ShapeBounding, -bw, -bw, p, ShapeSet);
    XFreeGC(d, gp);
    XFreePixmap(d, p);
}

void window_make_intransparent(XConnection& X, Window win) {
    Display* d = X.display();
    XShapeCombineMask(d, win, ShapeBounding, 0, 0, None, ShapeSet);
}


Point2D get_cursor_position() {
    Point2D point{};
    Window win, child;
    int wx, wy;
    unsigned int mask;
    if (True != XQueryPointer(g_display, g_root, &win, &child,
                              &point.x, &point.y, &wx,&wy, &mask)) {
        HSWarning("Cannot query cursor coordinates via XQueryPointer\n");
        point.x = 0;
        point.y = 0;
    }
    return point;
}
