#ifndef __CLIENTLIST_H_
#define __CLIENTLIST_H_

#include <X11/X.h>
#include <X11/Xlib.h>

#include "attribute_.h"
#include "child.h"
#include "commandio.h"
#include "object.h"
#include "rectangle.h"
#include "regexstr.h"
#include "theme.h"
#include "converter.h"
#include "x11-types.h"

class Decoration;
class DecTriple;
class Ewmh;
class FrameLeaf;
class Slice;
class HSTag;
class Monitor;
class Settings;
class ClientManager;
class XConnection;

class Client : public Object {
public:
    Client(Window w, bool visible_already, ClientManager& cm);
    ~Client() override;

    Window      window_;
    std::unique_ptr<Decoration> dec; // pimpl
    Rectangle   last_size_;      // last size excluding the window border
    Rectangle   float_size_ = {0, 0, 100, 100};     // floating size without the window border
    HSTag*      tag_ = {};
    Slice* slice = {};
    bool        ewmhfullscreen_ = false; // ewmh fullscreen state
    bool        neverfocus_ = false; // do not give the focus via XSetInputFocus
    Attribute_<bool> visible_;
    bool        dragged_ = false;  // if this client is dragged currently
    int         ignore_unmaps_ = 0;  // Ignore one unmap for each reparenting
                                // action, because reparenting creates an unmap
                                // notify event
    // for size hints
    float mina_ = 0;
    float maxa_ = 0;
    int basew_ = 0;
    int baseh_ = 0;
    int incw_ = 0;
    int inch_ = 0;
    int maxw_ = 0;
    int maxh_ = 0;
    int minw_ = 0;
    int minh_ = 0;

    // for other modules
    Signal_<HSTag*> needsRelayout;

    // attributes:
    Attribute_<bool> urgent_;
    Attribute_<bool> floating_;
    Attribute_<bool> fullscreen_;
    Attribute_<bool> minimized_;
    Attribute_<std::string> title_;  // or also called window title; this is never NULL
    DynAttribute_<std::string> tag_str_;
    DynChild_<FrameLeaf> parent_frame_;
    Attribute_<std::string> window_id_str;
    Attribute_<RegexStr> keyMask_; // regex for key bindings that are active on this window
    Attribute_<RegexStr> keysInactive_; // regex for key bindings that are inactive on this window
    Attribute_<int>  pid_;
    Attribute_<int>  pgid_;
    Attribute_<bool> pseudotile_; // only move client but don't resize (if possible)
    Attribute_<bool> ewmhrequests_; // accept ewmh-requests for this client
    Attribute_<bool> ewmhnotify_; // send ewmh-notifications for this client
    Attribute_<bool> sizehints_floating_;  // respect size hints regarding this client in floating mode
    Attribute_<bool> sizehints_tiling_;  // respect size hints regarding this client in tiling mode
    DynAttribute_<std::string> window_class_;
    DynAttribute_<std::string> window_instance_;

public:
    void init_from_X();

    void make_full_client();
    void listen_for_events();


    // setter and getter for attributes
    HSTag* tag() { return tag_; }
    void setTag(HSTag* tag);

    Window x11Window() const { return window_; }
    Window decorationWindow();
    friend void mouse_function_resize(XMotionEvent* me);

    // other member functions
    void window_focus();
    void window_unfocus();
    static void window_unfocus_last();

    void fuzzy_fix_initial_position();

    Rectangle outer_floating_rect();

    void setup_border(bool focused);
    void resize_tiling(Rectangle rect, bool isFocused, bool minimalDecoration);
    void resize_floating(Monitor* m, bool isFocused);
    void resize_fullscreen(Rectangle m, bool isFocused);
    bool is_client_floated();
    void set_urgent(bool state);
    void update_wm_hints();
    void update_title();
    void raise();
    void lower();

    void send_configure();
    bool applysizehints(int *w, int *h);
    void updatesizehints();

    void set_visible(bool visible);

    void set_urgent_force(bool state);
    void requestClose(); //! ask the client to close

    void clear_properties();
    bool ignore_unmapnotify();

    void updateEwmhState();
private:
    std::string getWindowClass();
    std::string getWindowInstance();
    std::string triggerRelayoutMonitor();
    FrameLeaf* parentFrame();
    void requestRedraw();
    friend Decoration;
    ClientManager& manager;
    Theme& theme;
    Settings& settings;
    Ewmh& ewmh;
    XConnection& X_;
    std::string tagName();
    const DecTriple& getDecTriple();
    Theme::Type mostRecentThemeType;
};



void reset_client_colors();

Client* get_client_from_window(Window window);
Client* get_current_client();
Client* get_client(const char* str);
Window get_window(const std::string& str);

int close_command(Input input, Output output);

// sets a client property, depending on argv[0]
int client_set_property_command(int argc, char** argv);
bool is_window_class_ignored(char* window_class);
bool is_window_ignored(Window win);

#endif
