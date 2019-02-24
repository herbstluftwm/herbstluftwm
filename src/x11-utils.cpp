#include "x11-utils.h"

#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/shapeconst.h>

#include "globals.h"

/**
 * \brief   cut a rect out of the window, s.t. the window has geometry rect and
 * a frame of width framewidth remains
 */
void window_cut_rect_hole(Window win, int width, int height, int framewidth) {
    // inspired by the xhole.c example
    // http://www.answers.com/topic/xhole-c
    Display* d = g_display;
    GC gp;
    int bw = 100; // add a large border, just to be sure the border is visible
    int holewidth = width - 2*framewidth;
    int holeheight = height - 2*framewidth;
    width += 2*bw;
    height += 2*bw;

    /* create the pixmap that specifies the shape */
    Pixmap p = XCreatePixmap(d, win, width, height, 1);
    gp = XCreateGC(d, p, 0, nullptr);
    XSetForeground(d, gp, WhitePixel(d, g_screen));
    XFillRectangle(d, p, gp, 0, 0, width, height);
    XSetForeground(d, gp, BlackPixel(d, g_screen));
    //int radius = 50;
    //XFillArc(d, p, gp,
    //         width/2 - radius, height/2 - radius, radius, radius,
    //         0, 360*64);
    XFillRectangle(d, p, gp, bw + framewidth, bw + framewidth,
                             holewidth, holeheight);

    /* set the pixmap as the new window mask;
    the pixmap is slightly larger than the window to allow for the window
    border and title bar (as added by the window manager) to be visible */
    XShapeCombineMask(d, win, ShapeBounding, -bw, -bw, p, ShapeSet);
    XFreeGC(d, gp);
    XFreePixmap(d, p);
}

void window_make_intransparent(Window win, int width, int height) {
    // inspired by the xhole.c example
    // http://www.answers.com/topic/xhole-c
    Display* d = g_display;
    GC gp;
    int bw = 100; // add a large border, just to be sure the border is visible
    width += 2*bw;
    height += 2*bw;

    /* create the pixmap that specifies the shape */
    Pixmap p = XCreatePixmap(d, win, width, height, 1);
    gp = XCreateGC(d, p, 0, nullptr);
    XSetForeground(d, gp, WhitePixel(d, g_screen));
    XFillRectangle(d, p, gp, 0, 0, width, height);
    /* set the pixmap as the new window mask;
    the pixmap is slightly larger than the window to allow for the window
    border and title bar (as added by the window manager) to be visible */
    XShapeCombineMask(d, win, ShapeBounding, -bw, -bw, p, ShapeSet);
    XFreeGC(d, gp);
    XFreePixmap(d, p);
}


Point2D get_cursor_position() {
    Point2D point;
    Window win, child;
    int wx, wy;
    unsigned int mask;
    if (True != XQueryPointer(g_display, g_root, &win, &child,
                              &point.x, &point.y, &wx,&wy, &mask)) {
        HSWarning("Can not query cursor coordinates via XQueryPointer\n");
        point.x = 0;
        point.y = 0;
    }
    return point;
}
