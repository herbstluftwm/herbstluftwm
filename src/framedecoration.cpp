#include "framedecoration.h"

#include "tag.h"
#include "stack.h"
#include "settings.h"
#include "globals.h"
#include "x11-utils.h"
#include "utils.h"
#include "ewmh.h"
#include "client.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

using namespace std;

static int* g_frame_border_width;
static int* g_frame_border_inner_width;
static int* g_always_show_frame;
static unsigned long g_frame_active_opacity;
static unsigned long g_frame_normal_opacity;
static int* g_frame_bg_transparent;
static int* g_frame_transparent_width;
static int* g_smart_frame_surroundings;

static void fetch_frame_colors() {
    g_frame_border_width = &(settings_find("frame_border_width")->value.i);
    g_frame_border_inner_width = &(settings_find("frame_border_inner_width")->value.i);
    g_always_show_frame = &(settings_find("always_show_frame")->value.i);
    g_frame_active_opacity = CLAMP(settings_find("frame_active_opacity")->value.i, 0, 100);
    g_frame_normal_opacity = CLAMP(settings_find("frame_normal_opacity")->value.i, 0, 100);
    g_frame_bg_transparent = &(settings_find("frame_bg_transparent")->value.i);
    g_frame_transparent_width = &(settings_find("frame_transparent_width")->value.i);
    g_smart_frame_surroundings = &(settings_find("smart_frame_surroundings")->value.i);
}


FrameDecorationData::FrameDecorationData()
    : visible(false)
    , hasClients(false)
    , hasParent(false)
{
}

FrameDecoration::FrameDecoration(HSTag* tag_)
    : visible(false)
    , window_transparent(false)
    , tag(tag_)
{
    fetch_frame_colors();
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
                        42, 42, 42, 42, *g_frame_border_width,
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
    stack_insert_slice(tag->stack, slice);
}

FrameDecoration::~FrameDecoration() {
    XDestroyWindow(g_display, window);
    stack_remove_slice(tag->stack, slice);
    slice_destroy(slice);
}

void FrameDecoration::render(const FrameDecorationData& data, bool isFocused) {
    fetch_frame_colors();
    unsigned long border_color = g_settings->frame_border_normal_color->toX11Pixel();
    unsigned long bg_color = g_settings->frame_bg_normal_color->toX11Pixel();
    int bw = *g_frame_border_width;
    if (isFocused) {
        border_color = g_settings->frame_border_active_color->toX11Pixel();
        bg_color = g_settings->frame_bg_active_color->toX11Pixel();
    }
    if (*g_smart_frame_surroundings && !data.hasParent) {
        bw = 0;
    }
    Rectangle rect = data.geometry;
    XSetWindowBorderWidth(g_display, window, bw);
    XMoveResizeWindow(g_display, window,
                      rect.x - bw,
                      rect.y - bw,
                      rect.width, rect.height);

    if (*g_frame_border_inner_width > 0
        && g_settings->frame_border_inner_width() < g_settings->frame_border_width()) {
        set_window_double_border(g_display, window,
                g_settings->frame_border_inner_width(),
                g_settings->frame_border_inner_color->toX11Pixel(),
                border_color);
    } else {
        XSetWindowBorder(g_display, window, border_color);
    }

    XSetWindowBackground(g_display, window, bg_color);
    if (*g_frame_bg_transparent) {
        window_cut_rect_hole(window, rect.width, rect.height,
                             *g_frame_transparent_width);
    } else if (window_transparent) {
        window_make_intransparent(window, rect.width, rect.height);
    }
    window_transparent = *g_frame_bg_transparent;
    if (isFocused) {
        ewmh_set_window_opacity(window, g_frame_active_opacity/100.0);
    } else {
        ewmh_set_window_opacity(window, g_frame_normal_opacity/100.0);
    }

    XClearWindow(g_display, window);
}

void FrameDecoration::updateVisibility(const FrameDecorationData& data, bool isFocused)
{
    bool show = *g_always_show_frame
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

