/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */


#ifndef __CLIENTLIST_H_
#define __CLIENTLIST_H_

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <glib.h>
#include <stdbool.h>

#include "layout.h"

typedef struct HSClient {
    Window      window;
    XRectangle  last_size;
    HSTag*      tag;
    XRectangle  float_size;
} HSClient;

void clientlist_init();
void clientlist_destroy();

void clientlist_foreach(GHFunc func, gpointer data);

void window_focus(Window window);
void window_unfocus(Window window);
void window_unfocus_last();

void reset_client_colors();
void reset_client_settings();

// adds a new client to list of managed client windows
void manage_client(Window win);
void unmanage_client(Window win);

void window_enforce_last_size(Window in);

// destroys a special client
void destroy_client(HSClient* client);

HSClient* get_client_from_window(Window window);
XRectangle client_outer_floating_rect(HSClient* client);

void client_resize(HSClient* client, XRectangle rect);
void client_resize_floating(HSClient* client, HSMonitor* m);
int window_close_current();

bool is_window_class_ignored(char* window_class);
bool is_window_ignored(Window win);

void window_show(Window win);
void window_hide(Window win);
void window_set_visible(Window win, bool visible);


// some globals
unsigned long g_window_border_active_color;
unsigned long g_window_border_normal_color;

#endif


