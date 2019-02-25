#ifndef __CLIENTLIST_H_
#define __CLIENTLIST_H_

#include <X11/X.h>
#include <X11/Xlib.h>

#include "attribute_.h"
#include "decoration.h"
#include "object.h"
#include "stack.h"
#include "x11-types.h"

class HSTag;
class Monitor;
class Settings;
class ClientManager;

class Client : public Object {
public:
    Client(Window w, bool already_visible, ClientManager& cm);
    ~Client() override;

    Window      window_;
    Decoration  dec;
    Rectangle   float_size_ = {0, 0, 100, 100};     // floating size without the window border
    Attribute_<bool> urgent_ = {"urgent", false};
    Attribute_<bool> fullscreen_ = {"fullscreen", false};
    Attribute_<std::string> title_ = {"title", {}};  // or also called window title; this is never NULL
    Slice* slice = {};
    Rectangle   last_size_;      // last size excluding the window border
    Attribute_<std::string> window_id_str = {"winid", {}};

    HSTag*      tag_ = {};
    Attribute_<
    std::string> keyMask_ = {"keymask", {}}; // keymask applied to mask out keybindins
    bool        ewmhfullscreen_ = false; // ewmh fullscreen state
    Attribute_<bool> pseudotile_ = {"pseudotile", false}; // only move client but don't resize (if possible)
    bool        neverfocus_ = false; // do not give the focus via XSetInputFocus
    bool        ewmhrequests_ = true; // accept ewmh-requests for this client
    bool        ewmhnotify_ = true; // send ewmh-notifications for this client
    bool        sizehints_floating_ = true;  // respect size hints regarding this client in floating mode
    bool        sizehints_tiling_ = false;  // respect size hints regarding this client in tiling mode
    bool        visible_;
    bool        dragged_ = false;  // if this client is dragged currently
    int         pid_;
    int         ignore_unmaps_ = 0;  // Ignore one unmap for each reparenting
                                // action, because reparenting creates an unmap
                                // notify event
    // for size hints
    float mina_, maxa_;
    int basew_, baseh_, incw_, inch_, maxw_, maxh_, minw_, minh_;
    // for other modules
    Signal_<HSTag*> needsRelayout;
    void init_from_X();

    void make_full_client();


    // setter and getter for attributes
    HSTag* tag() { return tag_; }
    void setTag(HSTag* tag) { tag_ = tag; }

    Window x11Window() { return window_; }
    Window decorationWindow() { return dec.decorationWindow(); }
    friend void mouse_function_resize(XMotionEvent* me);

    // other member functions
    void window_focus();
    void window_unfocus();
    static void window_unfocus_last();

    void fuzzy_fix_initial_position();

    Rectangle outer_floating_rect();

    void setup_border(bool focused);
    void resize_tiling(Rectangle rect, bool isFocused);
    void resize_floating(Monitor* m, bool isFocused);
    void resize_fullscreen(Rectangle m, bool isFocused);
    bool is_client_floated();
    bool needs_minimal_dec();
    void set_urgent(bool state);
    void update_wm_hints();
    void update_title();
    void raise();

    void set_dragged(bool drag_state);

    void send_configure();
    bool applysizehints(int *w, int *h);
    bool applysizehints_xy(int *x, int *y, int *w, int *h);
    void updatesizehints();

    bool sendevent(Atom proto);

    void set_visible(bool visible_);

    void set_fullscreen(bool state);
    void set_pseudotile(bool state);
    void set_urgent_force(bool state);

    void clear_properties();
    bool ignore_unmapnotify();

private:
    std::string triggerRelayoutMonitor();
    friend Decoration;
    ClientManager& manager;
    Theme& theme;
    Settings& settings;
    const DecTriple& getDecTriple();
};



void clientlist_init();
void clientlist_destroy();


void reset_client_colors();
void reset_client_settings();

Client* get_client_from_window(Window window);
Client* get_current_client();
Client* get_client(const char* str);
Window get_window(const std::string& str);

int close_command(Input input, Output output);
void window_close(Window window);

// sets a client property, depending on argv[0]
int client_set_property_command(int argc, char** argv);
bool is_window_class_ignored(char* window_class);
bool is_window_ignored(Window win);

void window_set_visible(Window win, bool visible);

#endif
