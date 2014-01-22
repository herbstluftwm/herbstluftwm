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

struct HSClient;


typedef struct {
    int     border_width;
    HSColor border_color;
    bool    tight_decoration; // if set, there is no space between the
                              // decoration and the window content
    HSColor inner_color;
    int     inner_width;
    HSColor outer_color;
    int     outer_width;
    int     padding_top;    // additional window border
    int     padding_right;  // additional window border
    int     padding_bottom; // additional window border
    int     padding_left;   // additional window border
} HSDecorationScheme;

typedef struct {
    struct HSClient*        client; // the client to decorate
    Window                  decwin; // the decoration winodw
    HSDecorationScheme      last_scheme;
    bool                    last_rect_inner; // whether last_rect is inner size
    Rectangle               last_inner_rect; // only valid if width >= 0
    Rectangle               last_outer_rect; // only valid if width >= 0
    GC                      gc;
} HSDecoration;

typedef struct {
    HSDecorationScheme  normal;
    HSDecorationScheme  active;
    HSDecorationScheme  urgent;
    HSObject            obj_normal;
    HSObject            obj_active;
    HSObject            obj_urgent;
    HSObject            object;
} HSDecTripple;

enum {
    HSDecSchemeFullscreen,
    HSDecSchemeTiling,
    HSDecSchemeFloating,
    HSDecSchemeCount,
};

extern HSDecTripple g_decorations[];

void decorations_init();
void decorations_destroy();

void decoration_init(HSDecoration* dec, struct HSClient* client);
void decoration_free(HSDecoration* dec);

// resize such that the decorated outline of the window fits into rect
void decoration_resize_outline(struct HSClient* client, Rectangle rect,
                               HSDecorationScheme scheme);

// resize such that the window content fits into rect
void decoration_resize_inner(struct HSClient* client, Rectangle rect,
                             HSDecorationScheme scheme);

void decoration_change_scheme(struct HSClient* client,
                              HSDecorationScheme scheme);

void decoration_redraw(struct HSClient* client);
struct HSClient* get_client_from_decoration(Window decwin);

Rectangle inner_rect_to_outline(Rectangle rect, HSDecorationScheme scheme);
Rectangle outline_to_inner_rect(Rectangle rect, HSDecorationScheme scheme);

#endif

