/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __CLIENTLIST_H_
#define __CLIENTLIST_H_

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include "glib-backports.h"
#include <stdbool.h>

#include "layout.h"
#include "object.h"
#include "utils.h"
#include "decoration.h"

struct HSSlice;

typedef struct HSClient {
    Window      window;
    GString*    window_str;     // the window id as a string
    Rectangle   last_size;      // last size excluding the window border
    HSTag*      tag;
    Rectangle   float_size;     // floating size without the window border
    GString*    title;  // or also called window title; this is never NULL
    GString*    keymask; // keymask applied to mask out keybindins
    bool        urgent;
    bool        fullscreen;
    bool        ewmhfullscreen; // ewmh fullscreen state
    bool        pseudotile; // only move client but don't resize (if possible)
    bool        neverfocus; // do not give the focus via XSetInputFocus
    bool        ewmhrequests; // accept ewmh-requests for this client
    bool        ewmhnotify; // send ewmh-notifications for this client
    bool        sizehints_floating;  // respect size hints regarding this client in floating mode
    bool        sizehints_tiling;  // respect size hints regarding this client in tiling mode
    bool        dragged;  // if this client is dragged currently
    int         pid;
    int         ignore_unmaps;  // Ignore one unmap for each reparenting
                                // action, because reparenting creates an unmap
                                // notify event
    bool        visible;
    // for size hints
	float mina, maxa;
    int basew, baseh, incw, inch, maxw, maxh, minw, minh;
    // for other modules
    HSObject    object;
    struct HSSlice* slice;
    HSDecoration    dec;
} HSClient;



void clientlist_init();
void clientlist_destroy();
void clientlist_end_startup();

bool clientlist_ignore_unmapnotify(Window win);

void clientlist_foreach(GHFunc func, gpointer data);

void client_window_focus(HSClient* client);
void client_window_unfocus(HSClient* client);
void client_window_unfocus_last();

void reset_client_colors();
void reset_client_settings();

// adds a new client to list of managed client windows
HSClient* manage_client(Window win, bool force_unmanage = false);
void client_fuzzy_fix_initial_position(HSClient* client);
void unmanage_client(Window win);

void window_enforce_last_size(Window in);

// destroys a special client
void client_destroy(HSClient* client);

HSClient* get_client_from_window(Window window);
HSClient* get_current_client();
HSClient* get_urgent_client();
Rectangle client_outer_floating_rect(HSClient* client);

Window string_to_client(const char* str, HSClient** ret_client);
void client_setup_border(HSClient* client, bool focused);
void client_resize(HSClient* client, Rectangle rect, HSFrame* frame);
void client_resize_tiling(HSClient* client, Rectangle rect, HSFrame* frame);
void client_resize_floating(HSClient* client, HSMonitor* m);
bool is_client_floated(HSClient* client);
bool client_needs_minimal_dec(HSClient* client, HSFrame* frame);
void client_set_urgent(HSClient* client, bool state);
void client_update_wm_hints(HSClient* client);
void client_update_title(HSClient* client);
void client_raise(HSClient* client);
int close_command(int argc, char** argv, GString* output);
void window_close(Window window);

void client_set_dragged(HSClient* client, bool drag_state);

void client_send_configure(HSClient *c);
bool applysizehints(HSClient *c, int *w, int *h);
bool applysizehints_xy(HSClient *c, int *x, int *y, int *w, int *h);
void updatesizehints(HSClient *c);

bool client_sendevent(HSClient *client, Atom proto);

void client_set_fullscreen(HSClient* client, bool state);
void client_set_pseudotile(HSClient* client, bool state);
// sets a client property, depending on argv[0]
int client_set_property_command(int argc, char** argv);
bool is_window_class_ignored(char* window_class);
bool is_window_ignored(Window win);

void client_set_visible(HSClient* client, bool visible);
void window_set_visible(Window win, bool visible);

unsigned long get_window_border_color(HSClient* client);

#endif
