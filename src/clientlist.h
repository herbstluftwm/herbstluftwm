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

class HSClient : public herbstluft::Object {
public:

    HSDecoration    dec;
    herbstluft::Rectangle   float_size_;     // floating size without the window border
    bool        urgent_;
    bool        fullscreen_;
    GString*    title_;  // or also called window title; this is never NULL
    struct HSSlice* slice;

public:
    Window      window_;
    GString*    window_str_;     // the window id as a string
    herbstluft::Rectangle   last_size_;      // last size excluding the window border
    HSTag*      tag_;
    GString*    keymask_; // keymask applied to mask out keybindins
    bool        ewmhfullscreen_; // ewmh fullscreen state
    bool        pseudotile_; // only move client but don't resize (if possible)
    bool        neverfocus_; // do not give the focus via XSetInputFocus
    bool        ewmhrequests_; // accept ewmh-requests for this client
    bool        ewmhnotify_; // send ewmh-notifications for this client
    bool        sizehints_floating_;  // respect size hints regarding this client in floating mode
    bool        sizehints_tiling_;  // respect size hints regarding this client in tiling mode
    bool        visible_;
    bool        dragged_;  // if this client is dragged currently
    int         pid_;
    int         ignore_unmaps_;  // Ignore one unmap for each reparenting
                                // action, because reparenting creates an unmap
                                // notify event
    // for size hints
    float mina_, maxa_;
    unsigned int basew_, baseh_, incw_, inch_, maxw_, maxh_, minw_, minh_;
    // for other modules
    HSObject    object;

public:

    HSClient();
    HSClient(Window window_);
    ~HSClient();


    // setter and getter for attributes
    HSTag* tag() { return tag_; };
    void setTag(HSTag* tag) { tag_ = tag; }

    Window x11Window() { return window_; };
    friend void mouse_function_resize(XMotionEvent* me);

    // other member functions
    void window_focus();
    void window_unfocus();
    static void window_unfocus_last();

    void fuzzy_fix_initial_position();

    herbstluft::Rectangle outer_floating_rect();

    void setup_border(bool focused);
    void resize(herbstluft::Rectangle rect, HSFrame* frame);
    void resize_tiling(herbstluft::Rectangle rect, HSFrame* frame);
    void resize_floating(HSMonitor* m);
    bool is_client_floated();
    bool needs_minimal_dec(HSFrame* frame);
    void set_urgent(bool state);
    void update_wm_hints();
    void update_title();
    void raise();

    void set_dragged(bool drag_state);

    void send_configure();
    bool applysizehints(unsigned int *w, unsigned int *h);
    bool applysizehints_xy(int *x, int *y, unsigned int *w, unsigned int *h);
    void updatesizehints();

    bool sendevent(Atom proto);

    void set_visible(bool visible_);

    bool ignore_unmapnotify();

    void set_fullscreen(bool state);
    void set_pseudotile(bool state);
    void set_urgent_force(bool state);

private:
    void resize_fullscreen(HSMonitor* m);
};



void clientlist_init();
void clientlist_destroy();
void clientlist_end_startup();

void clientlist_foreach(GHFunc func, gpointer data);

bool clientlist_ignore_unmapnotify(Window win);

void reset_client_colors();
void reset_client_settings();

// adds a new client to list of managed client windows
HSClient* manage_client(Window win);
void unmanage_client(Window win);

void window_enforce_last_size(Window in);

HSClient* get_client_from_window(Window window);
HSClient* get_current_client();
HSClient* get_urgent_client();

Window string_to_client(const char* str, HSClient** ret_client);
int close_command(int argc, char** argv, GString* output);
void window_close(Window window);

// sets a client property, depending on argv[0]
int client_set_property_command(int argc, char** argv);
bool is_window_class_ignored(char* window_class);
bool is_window_ignored(Window win);

void window_set_visible(Window win, bool visible);

#endif
