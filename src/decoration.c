
#include "decoration.h"
#include "globals.h"

void decoration_init(HSDecoration* dec, struct HSClient* client) {
    dec->client = client;
    XSetWindowAttributes at;
    at.background_pixel  = getcolor("red");
    at.background_pixmap = ParentRelative;
    at.override_redirect = True;
    at.bit_gravity       = StaticGravity;
    at.event_mask        = SubstructureRedirectMask|SubstructureNotifyMask
         |ExposureMask|VisibilityChangeMask
         |EnterWindowMask|LeaveWindowMask|FocusChangeMask;
    dec->decwin = XCreateWindow(g_display, g_root, 0,0, 30, 30, 0,
                        DefaultDepth(g_display, DefaultScreen(g_display)),
                        CopyFromParent,
                        DefaultVisual(g_display, DefaultScreen(g_display)),
                        CWOverrideRedirect | CWBackPixmap | CWEventMask, &at);
}

void decoration_free(HSDecoration* dec) {
    XDestroyWindow(g_display, dec->decwin);
}
