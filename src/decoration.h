/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __DECORATION_H_
#define __DECORATION_H_

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include "glib-backports.h"
#include "x11-utils.h"
#include "object.h"
#include <stdbool.h>

#include "utils.h"

class HSClient;


typedef struct {
    int     border_width;
    herbstluft::Color border_color;
    bool    tight_decoration; // if set, there is no space between the
                              // decoration and the window content
    herbstluft::Color inner_color;
    int     inner_width;
    herbstluft::Color outer_color;
    int     outer_width;
    int     padding_top;    // additional window border
    int     padding_right;  // additional window border
    int     padding_bottom; // additional window border
    int     padding_left;   // additional window border
    herbstluft::Color background_color; // color behind client contents
} HSDecorationScheme;

typedef struct {
    HSClient*        client; // the client to decorate
    Window                  decwin; // the decoration winodw
    HSDecorationScheme      last_scheme;
    bool                    last_rect_inner; // whether last_rect is inner size
    herbstluft::Rectangle   last_inner_rect; // only valid if width >= 0
    herbstluft::Rectangle   last_outer_rect; // only valid if width >= 0
    herbstluft::Rectangle   last_actual_rect; // last actual client rect, relative to decoration
    /* X specific things */
    Colormap                colormap;
    unsigned int            depth;
    Pixmap                  pixmap;
    int                     pixmap_height;
    int                     pixmap_width;
    // fill the area behind client with another window that does nothing,
    // especially not repainting or background filling to avoid flicker on
    // unmap
    Window                  bgwin;
} HSDecoration;

typedef struct {
    HSDecorationScheme  normal;
    HSDecorationScheme  active;
    HSDecorationScheme  urgent;
    HSObject            obj_normal;
    HSObject            obj_active;
    HSObject            obj_urgent;
    HSDecorationScheme  propagate; // meta-scheme for propagating values to members
    HSObject            object;
} HSDecTriple;

enum {
    HSDecSchemeFullscreen,
    HSDecSchemeTiling,
    HSDecSchemeFloating,
    HSDecSchemeMinimal,
    HSDecSchemeCount,
};

extern HSDecTriple g_decorations[];

void decorations_init();
void decorations_destroy();

void decoration_init(HSDecoration* dec, HSClient* client);
void decoration_setup_frame(HSClient* client);
void decoration_free(HSDecoration* dec);

// resize such that the decorated outline of the window fits into rect
void decoration_resize_outline(HSClient* client,
                               herbstluft::Rectangle rect,
                               HSDecorationScheme scheme);

// resize such that the window content fits into rect
void decoration_resize_inner(HSClient* client,
                             herbstluft::Rectangle rect,
                             HSDecorationScheme scheme);
void decoration_change_scheme(HSClient* client,
                              HSDecorationScheme scheme);

void decoration_redraw_pixmap(HSClient* client);
HSClient* get_client_from_decoration(Window decwin);

herbstluft::Rectangle inner_rect_to_outline(herbstluft::Rectangle rect,
                                            HSDecorationScheme scheme);
herbstluft::Rectangle outline_to_inner_rect(herbstluft::Rectangle rect,
                                            HSDecorationScheme scheme);

#endif

