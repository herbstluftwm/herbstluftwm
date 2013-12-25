
#include "decoration.h"
#include "clientlist.h"
#include "globals.h"
#include "settings.h"

#include <stdio.h>

HSDecTripple g_decorations[HSDecSchemeCount];

int* g_pseudotile_center_threshold;

void decorations_init() {
    g_pseudotile_center_threshold = &(settings_find("pseudotile_center_threshold")->value.i);
    // init default schemes
    HSDecTripple tiling = {
        { 3, getcolor("black"),     false },    // normal
        { 3, getcolor("green"),     false },    // active
        { 3, getcolor("orange"),    false },    // urgent
    };
    g_decorations[HSDecSchemeTiling] = tiling;
    HSDecTripple fs = {
        { 0, getcolor("black"),     false },    // normal
        { 0, getcolor("green"),     false },    // active
        { 0, getcolor("orange"),    false },    // urgent
    };
    g_decorations[HSDecSchemeFullscreen] = fs;
    HSDecTripple fl = {
        { 1, getcolor("black"),     false },    // normal
        { 4, getcolor("green"),     false },    // active
        { 1, getcolor("orange"),    false },    // urgent
    };
    g_decorations[HSDecSchemeFloating] = fl;
}

void decorations_destroy() {
}

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


static Rectangle outline_to_inner_rect(Rectangle rect, HSDecorationScheme scheme) {
    Rectangle inner = {
        .x = rect.x + scheme.border_width,
        .y = rect.y + scheme.border_width,
        .width  = rect.width  - 2* scheme.border_width,
        .height = rect.height - 2* scheme.border_width,
    };
    return inner;
}

static Rectangle inner_rect_to_outline(Rectangle rect, HSDecorationScheme scheme) {
    Rectangle out = {
        .x = rect.x - scheme.border_width,
        .y = rect.y - scheme.border_width,
        .width  = rect.width  + 2* scheme.border_width,
        .height = rect.height + 2* scheme.border_width,
    };
    return out;
}

void decoration_resize_inner(HSClient* client, Rectangle inner,
                             HSDecorationScheme scheme) {
    decoration_resize_outline(client, inner_rect_to_outline(inner, scheme), scheme);
}

void decoration_resize_outline(HSClient* client, Rectangle outline,
                               HSDecorationScheme scheme) {
    Rectangle inner = outline_to_inner_rect(outline, scheme);
    // get relative coordinates
    Window decwin = client->dec.decwin;
    Window win = client->window;

    //applysizehints(client, &inner.width, &inner.height);
    // center the window in the tile
    // but only if it's relative coordinates would not be too close to the
    // upper left tile border
    //int threshold = *g_pseudotile_center_threshold;
    //int dx = tile.width/2 - rect.width/2 - border_width;
    //int dy = tile.height/2 - rect.height/2 - border_width;
    //rect.x = tile.x + ((dx < threshold) ? 0 : dx);
    //rect.y = tile.y + ((dy < threshold) ? 0 : dy);

    //if (RECTANGLE_EQUALS(client->last_size, rect)
    //    && client->last_border_width == border_width) {
    //    return;
    //}

    inner.x -= outline.x;
    inner.y -= outline.y;
    XWindowChanges changes = {
      .x = inner.x, .y = inner.y, .width = inner.width, .height = inner.height,
      .border_width = 0
    };
    int mask = CWX | CWY | CWWidth | CWHeight | CWBorderWidth;
    XSetWindowBackground(g_display, decwin, scheme.border_color);
    XConfigureWindow(g_display, win, mask, &changes);
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
    client->last_size = inner;
    client_send_configure(client);
    XSync(g_display, False);
    XMoveResizeWindow(g_display, decwin,
                      outline.x, outline.y, outline.width, outline.height);
}


