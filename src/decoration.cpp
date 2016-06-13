
#include "decoration.h"
#include "client.h"
#include "globals.h"
#include "settings.h"
#include "ewmh.h"

#include <stdio.h>
#include <string.h>
#include <sstream>

using namespace herbstluft;

// public globals:
HSDecTriple g_decorations[HSDecSchemeCount];

// module intern globals:
static GHashTable* g_decwin2client = NULL;

static int* g_pseudotile_center_threshold;
static int* g_update_dragged_clients;
// dummy schemes for propagation
static HSDecorationScheme g_theme_scheme;
static HSDecorationScheme g_theme_active_scheme;
static HSDecorationScheme g_theme_normal_scheme;
static HSDecorationScheme g_theme_urgent_scheme;

// is called automatically after resize_outline
static void decoration_update_frame_extents(HSClient* client);

void decorations_init() {
    g_pseudotile_center_threshold = &(settings_find("pseudotile_center_threshold")->value.i);
    g_update_dragged_clients = &(settings_find("update_dragged_clients")->value.i);
    g_decwin2client = g_hash_table_new(g_int_hash, g_int_equal);
    // init default schemes
    // tiling //
    HSDecTriple tiling = {
        { 2, Color::fromStr("black"),     false },    // normal
        { 2, Color::fromStr("green"),     false },    // active
        { 2, Color::fromStr("orange"),    false },    // urgent
    };
    g_decorations[HSDecSchemeTiling] = tiling;
    // fullscreen //
    HSDecTriple fs = {
        { 0, Color::fromStr("black"),     false },    // normal
        { 0, Color::fromStr("black"),     false },    // active
        { 0, Color::fromStr("black"),     false },    // urgent
    };
    g_decorations[HSDecSchemeFullscreen] = fs;
    // floating //
    HSDecTriple fl = {
        { 1, Color::fromStr("black"),     true  },    // normal
        { 4, Color::fromStr("green"),     true  },    // active
        { 1, Color::fromStr("orange"),    true  },    // urgent
    };
    g_decorations[HSDecSchemeFloating] = fl;
    // minimal //
    HSDecTriple minimal = {
        { 0, Color::fromStr("black"),     true  },    // normal
        { 0, Color::fromStr("green"),     true  },    // active
        { 0, Color::fromStr("orange"),    true  },    // urgent
    };
    g_decorations[HSDecSchemeMinimal] = minimal;
    //init_dec_triple_object(g_decorations + HSDecSchemeTiling, "tiling");
    //init_dec_triple_object(g_decorations + HSDecSchemeFloating, "floating");
    //init_dec_triple_object(g_decorations + HSDecSchemeMinimal, "minimal");
    // create mass-attribute-objects
    g_theme_scheme
        = g_theme_active_scheme
        = g_theme_normal_scheme
        = g_theme_urgent_scheme = fs.normal;
}

void reset_helper(void* data, GString* output) {
    (void) data;
    g_string_append(output, "Writing this resets all attributes to a default value\n");
}

void decorations_destroy() {
    g_hash_table_destroy(g_decwin2client);
    g_decwin2client = NULL;
}

// from openbox/frame.c
static Visual* check_32bit_client(HSClient* c)
{
    XWindowAttributes wattrib;
    Status ret;

    ret = XGetWindowAttributes(g_display, c->window_, &wattrib);
    HSWeakAssert(ret != BadDrawable);
    HSWeakAssert(ret != BadWindow);

    if (wattrib.depth == 32)
        return wattrib.visual;
    return NULL;
}

void decoration_init(HSDecoration* dec, HSClient* client) {
    memset(dec, 0, sizeof(*dec));
    dec->client = client;
}

void decoration_setup_frame(HSClient* client) {
    HSDecoration* dec = &(client->dec);
    XSetWindowAttributes at;
    long mask = 0;
    // copy attributes from client and not from the root window
    Visual* visual = check_32bit_client(client);
    if (visual) {
        /* client has a 32-bit visual */
        mask = CWColormap | CWBackPixel | CWBorderPixel;
        /* create a colormap with the visual */
        dec->colormap = at.colormap =
            XCreateColormap(g_display, g_root, visual, AllocNone);
        at.background_pixel = BlackPixel(g_display, g_screen);
        at.border_pixel = BlackPixel(g_display, g_screen);
    } else {
        dec->colormap = 0;
    }
    dec->depth = visual
                 ? 32
                 : (DefaultDepth(g_display, DefaultScreen(g_display)));
    dec->decwin = XCreateWindow(g_display, g_root, 0,0, 30, 30, 0,
                        dec->depth,
                        InputOutput,
                        visual
                            ? visual
                            : DefaultVisual(g_display, DefaultScreen(g_display)),
                        mask, &at);
    mask = 0;
    if (visual) {
        /* client has a 32-bit visual */
        mask = CWColormap | CWBackPixel | CWBorderPixel;
        // TODO: why does DefaultColormap work in openbox but crashes hlwm here?
        // It somehow must be incompatible to the visual and thus causes the
        // BadMatch on XCreateWindow
        at.colormap = dec->colormap;
        at.background_pixel = BlackPixel(g_display, g_screen);
        at.border_pixel = BlackPixel(g_display, g_screen);
    }
    dec->bgwin = 0;
    dec->bgwin = XCreateWindow(g_display, dec->decwin, 0,0, 30, 30, 0,
                        dec->depth,
                        InputOutput,
                        CopyFromParent,
                        mask, &at);
    XMapWindow(g_display, dec->bgwin);
    // use a clients requested initial floating size as the initial size
    dec->last_rect_inner = true;
    dec->last_inner_rect = client->float_size_;
    dec->last_outer_rect = inner_rect_to_outline(client->float_size_, dec->last_scheme);
    dec->last_actual_rect = dec->last_inner_rect;
    dec->last_actual_rect.x -= dec->last_outer_rect.x;
    dec->last_actual_rect.y -= dec->last_outer_rect.y;
    dec->pixmap = 0;
    g_hash_table_insert(g_decwin2client, &(dec->decwin), client);
    // set wm_class for window
    XClassHint *hint = XAllocClassHint();
    hint->res_name = (char*)HERBST_DECORATION_CLASS;
    hint->res_class = (char*)HERBST_DECORATION_CLASS;
    XSetClassHint(g_display, dec->decwin, hint);
    XFree(hint);
}

void decoration_free(HSDecoration* dec) {
    if (g_decwin2client) {
        g_hash_table_remove(g_decwin2client, &(dec->decwin));
    }
    if (dec->colormap) {
        XFreeColormap(g_display, dec->colormap);
    }
    if (dec->pixmap) {
        XFreePixmap(g_display, dec->pixmap);
    }
    if (dec->bgwin) {
        XDestroyWindow(g_display, dec->bgwin);
    }
    if (dec->decwin) {
        XDestroyWindow(g_display, dec->decwin);
    }
}

HSClient* get_client_from_decoration(Window decwin) {
    return (HSClient*) g_hash_table_lookup(g_decwin2client, &decwin);
}

Rectangle outline_to_inner_rect(Rectangle rect, HSDecorationScheme s) {
    return {
        rect.x + s.border_width + s.padding_left,
        rect.y + s.border_width + s.padding_top,
        rect.width  - 2* s.border_width - s.padding_left - s.padding_right,
        rect.height - 2* s.border_width - s.padding_top - s.padding_bottom
    };
}

Rectangle inner_rect_to_outline(Rectangle rect, HSDecorationScheme s) {
    return {
        rect.x - s.border_width - s.padding_left,
        rect.y - s.border_width - s.padding_top,
        rect.width  + 2* s.border_width + s.padding_left + s.padding_right,
        rect.height + 2* s.border_width + s.padding_top + s.padding_bottom
    };
}

void decoration_resize_inner(HSClient* client, Rectangle inner,
                             HSDecorationScheme scheme) {
    decoration_resize_outline(client, inner_rect_to_outline(inner, scheme), scheme);
    client->dec.last_rect_inner = true;
}

void decoration_resize_outline(HSClient* client, Rectangle outline,
                               HSDecorationScheme scheme)
{
    auto inner = outline_to_inner_rect(outline, scheme);
    // get relative coordinates
    Window decwin = client->dec.decwin;
    Window win = client->window_;

    auto tile = inner;
    client->applysizehints(&inner.width, &inner.height);
    if (!scheme.tight_decoration) {
        // center the window in the outline tile
        // but only if it's relative coordinates would not be too close to the
        // upper left tile border
        int threshold = *g_pseudotile_center_threshold;
        int dx = tile.width/2 - inner.width/2;
        int dy = tile.height/2 - inner.height/2;
        inner.x = tile.x + ((dx < threshold) ? 0 : dx);
        inner.y = tile.y + ((dy < threshold) ? 0 : dy);
    }

    //if (RECTANGLE_EQUALS(client->last_size, rect)
    //    && client->last_border_width == border_width) {
    //    return;
    //}

    if (scheme.tight_decoration) {
        outline = inner_rect_to_outline(inner, scheme);
    }
    client->dec.last_inner_rect = inner;
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
    bool size_changed = outline.width != client->dec.last_outer_rect.width
                     || outline.height != client->dec.last_outer_rect.height;
    client->dec.last_outer_rect = outline;
    client->dec.last_rect_inner = false;
    client->last_size_ = inner;
    client->dec.last_scheme = scheme;
    // redraw
    // TODO: reduce flickering
    if (!client->dragged_ || *g_update_dragged_clients) {
        client->dec.last_actual_rect.x = changes.x;
        client->dec.last_actual_rect.y = changes.y;
        client->dec.last_actual_rect.width = changes.width;
        client->dec.last_actual_rect.height = changes.height;
    }
    decoration_redraw_pixmap(client);
    XSetWindowBackgroundPixmap(g_display, decwin, client->dec.pixmap);
    if (!size_changed) {
        // if size changes, then the window is cleared automatically
        XClearWindow(g_display, decwin);
    }
    if (!client->dragged_ || *g_update_dragged_clients) {
        XConfigureWindow(g_display, win, mask, &changes);
        XMoveResizeWindow(g_display, client->dec.bgwin,
                          changes.x, changes.y,
                          changes.width, changes.height);
    }
    XMoveResizeWindow(g_display, decwin,
                      outline.x, outline.y, outline.width, outline.height);
    decoration_update_frame_extents(client);
    if (!client->dragged_ || *g_update_dragged_clients) {
        client->send_configure();
    }
    XSync(g_display, False);
}

static void decoration_update_frame_extents(HSClient* client) {
    int left = client->dec.last_inner_rect.x - client->dec.last_outer_rect.x;
    int top  = client->dec.last_inner_rect.y - client->dec.last_outer_rect.y;
    int right = client->dec.last_outer_rect.width - client->dec.last_inner_rect.width - left;
    int bottom = client->dec.last_outer_rect.height - client->dec.last_inner_rect.height - top;
    ewmh_update_frame_extents(client->window_, left,right, top,bottom);
}

void decoration_change_scheme(HSClient* client,
                              HSDecorationScheme scheme) {
    if (client->dec.last_inner_rect.width < 0) {
        // TODO: do something useful here
        return;
    }
    if (client->dec.last_rect_inner) {
        decoration_resize_inner(client, client->dec.last_inner_rect, scheme);
    } else {
        decoration_resize_outline(client, client->dec.last_outer_rect, scheme);
    }
}

static unsigned int get_client_color(HSClient* client, unsigned int pixel) {
    if (client->dec.colormap) {
        XColor xcol;
        xcol.pixel = pixel;
        /* get rbg value out of default colormap */
        XQueryColor(g_display, DefaultColormap(g_display, g_screen), &xcol);
        /* get pixel value back appropriate for client */
        XAllocColor(g_display, client->dec.colormap, &xcol);
        return xcol.pixel;
    } else {
        return pixel;
    }
}

// draw a decoration to the client->dec.pixmap
void decoration_redraw_pixmap(HSClient* client) {
    HSDecorationScheme s = client->dec.last_scheme;
    HSDecoration *const dec = &client->dec;
    Window win = client->dec.decwin;
    auto outer = client->dec.last_outer_rect;
    unsigned int depth = client->dec.depth;
    // TODO: maybe do something like pixmap recreate threshhold?
    bool recreate_pixmap = (dec->pixmap == 0) || (dec->pixmap_width != outer.width)
                                              || (dec->pixmap_height != outer.height);
    if (recreate_pixmap) {
        if (dec->pixmap) {
            XFreePixmap(g_display, dec->pixmap);
        }
        dec->pixmap = XCreatePixmap(g_display, win, outer.width, outer.height, depth);
    }
    Pixmap pix = dec->pixmap;
    GC gc = XCreateGC(g_display, pix, 0, NULL);

    // draw background
    XSetForeground(g_display, gc, get_client_color(client, s.border_color));
    XFillRectangle(g_display, pix, gc, 0, 0, outer.width, outer.height);

    // Draw inner border
    int iw = s.inner_width;
    auto inner = client->dec.last_inner_rect;
    inner.x -= client->dec.last_outer_rect.x;
    inner.y -= client->dec.last_outer_rect.y;
    if (iw > 0) {
        /* fill rectangles because drawing does not work */
        XRectangle rects[] = {
            { inner.x - iw, inner.y - iw, inner.width + 2*iw, iw }, /* top */
            { inner.x - iw, inner.y, iw, inner.height },  /* left */
            { inner.x + inner.width, inner.y, iw, inner.height }, /* right */
            { inner.x - iw, inner.y + inner.height, inner.width + 2*iw, iw }, /* bottom */
        };
        XSetForeground(g_display, gc, get_client_color(client, s.inner_color));
        XFillRectangles(g_display, pix, gc, rects, LENGTH(rects));
    }

    // Draw outer border
    unsigned int ow = s.outer_width;
    outer.x -= client->dec.last_outer_rect.x;
    outer.y -= client->dec.last_outer_rect.y;
    if (ow > 0) {
        ow = std::min(ow, (outer.height+1) / 2);
        XRectangle rects[] = {
            { 0, 0, outer.width, ow }, /* top */
            { 0, ow, ow, outer.height - 2*ow }, /* left */
            { outer.width-ow, ow, ow, outer.height - 2*ow }, /* right */
            { 0, outer.height - ow, outer.width, ow }, /* bottom */
        };
        XSetForeground(g_display, gc, get_client_color(client, s.outer_color));
        XFillRectangles(g_display, pix, gc, rects, LENGTH(rects));
    }
    // fill inner rect that is not covered by the client
    XSetForeground(g_display, gc, get_client_color(client, s.background_color));
    if (dec->last_actual_rect.width < inner.width) {
        XFillRectangle(g_display, pix, gc,
                       dec->last_actual_rect.x + dec->last_actual_rect.width,
                       dec->last_actual_rect.y,
                       inner.width - dec->last_actual_rect.width,
                       dec->last_actual_rect.height);
    }
    if (dec->last_actual_rect.height < inner.height) {
        XFillRectangle(g_display, pix, gc,
                       dec->last_actual_rect.x,
                       dec->last_actual_rect.y + dec->last_actual_rect.height,
                       inner.width,
                       inner.height - dec->last_actual_rect.height);
    }
    // clean up
    XFreeGC(g_display, gc);
}

