#include "framedecoration.h"

#include "tag.h"
#include "stack.h"
#include "settings.h"
#include "globals.h"
#include "x11-utils.h"
#include "utils.h"
#include "ewmh.h"
#include "client.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

using namespace std;

FrameDecoration::FrameDecoration(HSTag* tag_, Settings* settings_)
    : visible(false)
    , window_transparent(false)
    , tag(tag_)
    , settings(settings_)
{
    // set window attributes
    XSetWindowAttributes at;
    at.background_pixel  = Color("red").toX11Pixel();
    at.background_pixmap = ParentRelative;
    at.override_redirect = True;
    at.bit_gravity       = StaticGravity;
    at.event_mask        = SubstructureRedirectMask|SubstructureNotifyMask
         |ExposureMask|VisibilityChangeMask
         |EnterWindowMask|LeaveWindowMask|FocusChangeMask;

    window = XCreateWindow(g_display, g_root,
                        42, 42, 42, 42, settings->frame_border_width(),
                        DefaultDepth(g_display, DefaultScreen(g_display)),
                        CopyFromParent,
                        DefaultVisual(g_display, DefaultScreen(g_display)),
                        CWOverrideRedirect | CWBackPixmap | CWEventMask, &at);

    // set wm_class for window
    XClassHint *hint = XAllocClassHint();
    hint->res_name = (char*)HERBST_FRAME_CLASS;
    hint->res_class = (char*)HERBST_FRAME_CLASS;
    XSetClassHint(g_display, window, hint);

    XFree(hint);

    // insert it to the stack
    slice = slice_create_frame(window);
    tag->stack->insert_slice(slice);
}

FrameDecoration::~FrameDecoration() {
    XDestroyWindow(g_display, window);
    tag->stack->remove_slice(slice);
    slice_destroy(slice);
}

void FrameDecoration::render(const FrameDecorationData& data, bool isFocused) {
    unsigned long border_color = settings->frame_border_normal_color->toX11Pixel();
    unsigned long bg_color = settings->frame_bg_normal_color->toX11Pixel();
    int bw = settings->frame_border_width();
    if (isFocused) {
        border_color = settings->frame_border_active_color->toX11Pixel();
        bg_color = settings->frame_bg_active_color->toX11Pixel();
    }
    if (settings->smart_frame_surroundings() && !data.hasParent) {
        bw = 0;
    }
    Rectangle rect = data.geometry;
    XSetWindowBorderWidth(g_display, window, bw);
    XMoveResizeWindow(g_display, window,
                      rect.x - bw,
                      rect.y - bw,
                      rect.width, rect.height);

    if (settings->frame_border_inner_width() > 0
        && settings->frame_border_inner_width() < settings->frame_border_width()) {
        set_window_double_border(g_display, window,
                settings->frame_border_inner_width(),
                settings->frame_border_inner_color->toX11Pixel(),
                border_color);
    } else {
        XSetWindowBorder(g_display, window, border_color);
    }

    XSetWindowBackground(g_display, window, bg_color);
    if (settings->frame_bg_transparent()) {
        window_cut_rect_hole(window, rect.width, rect.height,
                             settings->frame_transparent_width());
    } else if (window_transparent) {
        window_make_intransparent(window, rect.width, rect.height);
    }
    window_transparent = settings->frame_bg_transparent();
    if (isFocused) {
        ewmh_set_window_opacity(window, settings->frame_active_opacity()/100.0);
    } else {
        ewmh_set_window_opacity(window, settings->frame_normal_opacity()/100.0);
    }

    XClearWindow(g_display, window);
}

void FrameDecoration::updateVisibility(const FrameDecorationData& data, bool isFocused)
{
    bool show = settings->always_show_frame()
              || data.hasClients
              || isFocused;
    if (show != visible) {
        visible = show;
        window_set_visible(window, visible);
    }
}

void FrameDecoration::hide() {
    if (visible) {
        visible = false;
        window_set_visible(window, visible);
    }
}

