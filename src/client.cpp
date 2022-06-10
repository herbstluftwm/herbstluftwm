#include "client.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "clientmanager.h"
#include "decoration.h"
#include "ewmh.h"
#include "frametree.h"
#include "globals.h"
#include "hook.h"
#include "ipc-protocol.h"
#include "keymanager.h"
#include "layout.h"
#include "monitor.h"
#include "monitormanager.h"
#include "mousemanager.h"
#include "root.h"
#include "stack.h"
#include "tag.h"
#include "theme.h"
#include "utils.h"
#include "xconnection.h"

using std::pair;
using std::vector;
using std::shared_ptr;
using std::string;
using std::stringstream;

Client::Client(Window window, bool visible_already, ClientManager& cm)
    : window_(window)
    , dec(make_unique<Decoration>(this, *cm.settings, *cm.theme))
    , float_size_(this, "floating_geometry",  {0, 0, 100, 100})
    , decorated_(this, "decorated", true)
    , visible_(this, "visible", visible_already)
    , urgent_(this, "urgent", false)
    , floating_(this,  "floating", false)
    , fullscreen_(this,  "fullscreen", false)
    , minimized_(this,  "minimized", false)
    , floating_effectively_(this,  "floating_effectively", false)
    , title_(this,  "title", "")
    , tag_str_(this,  "tag", &Client::tagName)
    , parent_frame_(*this,  "parent_frame", &Client::parentFrame)
    , window_id_str(this,  "winid", "")
    , keyMask_(this,  "keymask", RegexStr::fromStr(""))
    , keysInactive_(this,  "keys_inactive", RegexStr::fromStr(""))
    , pid_(this,  "pid", -1)
    , pgid_(this, "pgid", -1)
    , pseudotile_(this,  "pseudotile", false)
    , ewmhrequests_(this, "ewmhrequests", true)
    , ewmhnotify_(this, "ewmhnotify", true)
    , sizehints_floating_(this, "sizehints_floating", true)
    , sizehints_tiling_(this, "sizehints_tiling", false)
    , window_class_(this, "class", &Client::getWindowClass)
    , window_instance_(this, "instance", &Client::getWindowInstance)
    , content_geometry_(this, "content_geometry", {})
    , decoration_geometry_(this, "decoration_geometry", &Client::decorationGeometry)
    , manager(cm)
    , theme(*cm.theme)
    , settings(*cm.settings)
    , ewmh(*cm.ewmh)
    , X_(*cm.X_)
    , decParams(make_unique<DecorationParameters>())
{
    stringstream tmp;
    window_id_str = WindowID(window).str();
    decorated_.setWritable();
    floating_.setWritable();
    keyMask_.setWritable();
    keysInactive_.setWritable();
    ewmhnotify_.setWritable();
    ewmhrequests_.setWritable();
    fullscreen_.setWritable();
    pseudotile_.setWritable();
    sizehints_floating_.setWritable();
    sizehints_tiling_.setWritable();
    minimized_.setWritable();
    urgent_.setWritable();
    for (auto i : {&fullscreen_, &pseudotile_, &sizehints_floating_, &sizehints_tiling_}) {
        i->changed().connect(this, &Client::requestRedraw);
    }

    keyMask_.changed().connect([this] {
            if (Root::get()->clients()->focus() == this) {
                Root::get()->keys()->ensureKeyMask();
            }
            });
    keysInactive_.changed().connect([this] {
            if (Root::get()->clients()->focus() == this) {
                Root::get()->keys()->ensureKeyMask();
            }
            });
    fullscreen_.changed().connect([this] {
        updateEwmhState();
        hook_emit({"fullscreen", fullscreen_() ? "on" : "off", WindowID(window_).str()});
    });
    minimized_.changed().connect([this]() {
        static long long minimizedTick = 0;
        minimizedLastChange_ = minimizedTick++;
        this->updateEwmhState();
    });

    float_size_.setWritable();
    float_size_.changedByUser().connect(this, &Client::floatingGeometryChanged);

    init_from_X();
    decorated_.setDoc("whether window border and title are drawn");
    visible_.setDoc("whether this client is rendered currently");
    parent_frame_.setDoc("the frame contaning this client if the client is tiled");
    setDoc("a managed window");

    window_id_str.setDoc("its window id (as a hexadecimal number with 0x prefix)");
    title_.setDoc("its window title");
    keyMask_.setDoc(
        "A regular expression that is matched against the string "
        "representation of all key bindings (as they are printed "
        "by list_keybinds). While this client is focused, only "
        "bindings that match the expression will be active. "
        "Any other bindings will be disabled. The default keymask "
        "is an empty string (""), which does not disable any keybinding.");
    keysInactive_.setDoc(
        "A regular expression that describes which keybindings are inactive "
        "while the client is focused. If a key combination is pressed and "
        "its string representation (as given by list_keybinds) matches the "
        "regex, then the key press is propagated to the client.");
    tag_str_.setDoc("the name of the tag it's currently on.");
    pid_.setDoc("the process id of it (-1 if unset).");
    window_class_.setDoc("the class of it (second entry in WM_CLASS)");
    window_instance_.setDoc("the instance of it (first entry in WM_CLASS)");
    fullscreen_.setDoc(
                "whether this client covers all other "
                "windows and panels on its monitor.");
    minimized_.setDoc(
                "whether this client is minimized (also called "
                "iconified).");
    floating_.setDoc("whether this client is set as a (single-window) floating client. "
                     "If set, the client is floated above the tiled clients.");
    floating_effectively_.setDoc(
                "whether this client is in the floating state currently. "
                "This is the case if the client\'s tag is set to floating mode or "
                "if the client itself is set as floating. Its value is also indicated "
                "via the X11 properties HLWM_FLOATING_WINDOW and HLWM_TILING_WINDOW.");
    pseudotile_.setDoc(
                "if activated, the client always has its floating "
                "window size, even if it is in tiling mode.");
    ewmhrequests_.setDoc("if ewmh requests are permitted for this client");
    ewmhnotify_.setDoc("if the client is told about its state via ewmh");
    urgent_.setDoc("the urgency state (also known as: demands attention). "
                   "The focused client can not be urgent.");
    sizehints_tiling_.setDoc("if sizehints for this client "
                             "should be respected in tiling mode");
    sizehints_floating_.setDoc("if sizehints for this client should "
                               "be respected in floating mode");
    float_size_.setDoc(
                "the geometry of the client content if the client is in "
                "floating mode. The position is relative to the monitor "
                "and does not take the window decoration into account.");
    content_geometry_.setDoc(
                "the geometry of the application content, that is, not taking "
                "the decoration into account. "
                "Also, this is the last window geometry that "
                "was reported to the client application.");
    decoration_geometry_.setDoc(
                "the geometry of the client, taking the window decoration "
                "into account. "
                "The position is the global window position, that is, "
                "relative to the top left corner of the entire screen");
}

void Client::init_from_X() {
    // treat wanted coordinates as floating coords
    auto root = Root::get();
    auto globalGeometry = root->X.windowSize(window_);
    float_size_ = root->monitors->interpretGlobalGeometry(globalGeometry);
    last_size_ = float_size_;

    pid_ = Root::get()->X.windowPid(window_);
    pgid_ = Root::get()->X.windowPgid(window_);

    update_title();
    readWmHints();
    updatesizehints();
}

void Client::make_full_client() {
    // setup decoration
    XSetWindowBorderWidth(X_.display(), window_, 0);
    // specify that the client window survives if hlwm dies, i.e. it will be
    // reparented back to root
    XChangeSaveSet(X_.display(), window_, SetModeInsert);
    XReparentWindow(X_.display(), window_, dec->decorationWindow(), 40, 40);
    if (visible_()) {
        // if this client is visible, then reparenting will make it invisible
        // and will create a unmap notify event.
        // the xmainloop only reacts to unmap events that come from the window
        // itself (event->event), and not from the root window. At the present
        // point, we did not call XSelectInput() yet, so the above
        // XReparentWindow does not trigger an event for the window.
        visible_ = false;
        // we also unmap the window such that the decoration window is
        // in 'mapped' state if and only if the window itself is so.
        XUnmapWindow(X_.display(), window_);
    }
    // get events from window
    XSelectInput(X_.display(), dec->decorationWindow(), (EnterWindowMask | LeaveWindowMask |
                            ButtonPressMask | ButtonReleaseMask |
                            ExposureMask |
                            SubstructureRedirectMask | FocusChangeMask));
    XSelectInput(X_.display(), window_,
                            StructureNotifyMask|FocusChangeMask
                            |EnterWindowMask|PropertyChangeMask);
    // redraw decoration on title change
    title_.changed().connect(this, &Client::redraw);
    title_.changed().connect(this, &Client::redrawRelevantTabBars);
    decorated_.changed().connect(this, &Client::fixParentWindow);
    urgent_.changed().connect(this, &Client::urgencyAttributeChanged);
}

void Client::listen_for_events() {
    XSelectInput(X_.display(), window_, PropertyChangeMask);
}

void Client::setTag(HSTag *tag) {
    tag_ = tag;
    ewmh.windowUpdateTag(window_, tag);
}

bool Client::ignore_unmapnotify() {
    if (ignore_unmaps_ > 0) {
        ignore_unmaps_--;
        return true;
    } else {
        return false;
    }
}

void reset_client_colors() {
    all_monitors_apply_layout();
}

Client* get_client_from_window(Window window) {
    return Root::get()->clients()->client(window);
}

// destroys a special client
Client::~Client() {
    if (slice) {
        delete slice;
    }
}

void Client::recomputeStyle()
{
    dec->setParameters(*decParams);
}

void Client::redraw()
{
    decParams->updateTabUrgencyFlags();
    dec->setParameters(*decParams);
    dec->redraw();
}

void Client::redrawRelevantTabBars()
{
    if (!is_client_floated()) {
        // if this client is mentioned in another client's tab bar,
        // then update also that
        FrameLeaf* parent = parentFrame();
        if (parent && parent->getLayout() == LayoutAlgorithm::max) {
            parent->foreachClient([&](Client* otherClient) {
                if (otherClient != this) {
                    otherClient->redraw();
                }
            });
        }
    }
}

void Client::resize_fullscreen(Rectangle monitor_rect, bool isFocused) {
    *decParams = DecorationParameters();
    decParams->focused_ = isFocused;
    decParams->fullscreen_ = true;
    decParams->urgent_ = urgent_();
    dec->setParameters(*decParams);
    dec->resize_outline(monitor_rect);
}

void Client::raise() {
    this->tag()->stack->raiseSlice(this->slice);
}

void Client::lower()
{
    this->tag()->stack->lowerSlice(this->slice);
}

/**
 * @brief Client::resize_tiling
 * @param the outer geometry of the client
 * @param whether this client has the focus
 * @param whether the client should use the 'minimal decoration' scheme
 * @param the tabs of the current window
 */
void Client::resize_tiling(Rectangle rect, bool isFocused, bool minimalDecoration, vector<Client*> tabs) {
    // only apply minimal decoration if the window is not pseudotiled
    *decParams = DecorationParameters();
    decParams->minimal_ = minimalDecoration && !this->pseudotile_();
    decParams->pseudotiled_ = this->pseudotile_();
    decParams->tabs_ = tabs;
    decParams->urgent_ = this->urgent_();
    decParams->urgentTabs_.reserve(tabs.size());
    decParams->focused_ = isFocused;
    for (const Client* tab : tabs) {
        decParams->urgentTabs_.push_back(tab->urgent_());
    }
    dec->setParameters(*decParams);
    if (this->pseudotile_) {
        Rectangle inner = this->float_size_;
        applysizehints(&inner.width, &inner.height);
        rect.x += std::max(0, (rect.width - inner.width)/2);
        rect.y += std::max(0, (rect.height - inner.height)/2);
        rect.width = std::min(inner.width, rect.width);
        rect.height = std::min(inner.height, rect.height);
    }
    dec->resize_outline(rect);
}

/**
 * @brief Update the given window size according to the client's size hints
 * @param w width
 * @param h height
 * @param force If set, always apply the size hints. If not set, only
 * apply the size hints if the according sizehints_floating_ / sizehints_tiling_
 * attribute is set
 * @return whether the size changed
 */
bool Client::applysizehints(int* w, int* h, bool force) {
    bool baseismin;

    /* set minimum possible */
    *w = std::max(1, *w);
    *h = std::max(1, *h);
    if (*h < WINDOW_MIN_HEIGHT) {
        *h = WINDOW_MIN_HEIGHT;
    }
    if (*w < WINDOW_MIN_WIDTH) {
        *w = WINDOW_MIN_WIDTH;
    }
    bool sizehints = force || (this->is_client_floated() || this->pseudotile_)
                        ? this->sizehints_floating_
                        : this->sizehints_tiling_;
    if (sizehints) {
        /* see last two sentences in ICCCM 4.1.2.3 */
        baseismin = this->basew_ == this->minw_ && this->baseh_ == this->minh_;
        if(!baseismin) { /* temporarily remove base dimensions */
            *w -= this->basew_;
            *h -= this->baseh_;
        }
        /* adjust for aspect limits */
        if(this->mina_ > 0 && this->maxa_ > 0) {
            if (this->maxa_ < (float)*w / *h) {
                *w = int(*h * this->maxa_ + 0.5f);
            } else if (this->mina_ < (float)*h / *w) {
                *h = int(*w * this->mina_ + 0.5f);
            }
        }
        if(baseismin) { /* increment calculation requires this */
            *w -= this->basew_;
            *h -= this->baseh_;
        }
        /* adjust for increment value */
        if (this->incw_) {
            *w -= *w % this->incw_;
        }
        if (this->inch_) {
            *h -= *h % this->inch_;
        }
        /* restore base dimensions */
        *w += this->basew_;
        *h += this->baseh_;
        *w = std::max(*w, this->minw_);
        *h = std::max(*h, this->minh_);
        if (this->maxw_) {
            *w = std::min(*w, this->maxw_);
        }
        if (this->maxh_) {
            *h = std::min(*h, this->maxh_);
        }
    }
    return *w != this->last_size_.width || *h != this->last_size_.height;
}

// from dwm.c
void Client::updatesizehints() {
    long msize;
    XSizeHints size;

    if (!XGetWMNormalHints(X_.display(), this->window_, &size, &msize)) {
        /* size is uninitialized, ensure that size.flags aren't used */
        size.flags = PSize;
    }
    if(size.flags & PBaseSize) {
        this->basew_ = size.base_width;
        this->baseh_ = size.base_height;
    }
    else if(size.flags & PMinSize) {
        this->basew_ = size.min_width;
        this->baseh_ = size.min_height;
    } else {
        this->basew_ = this->baseh_ = 0;
    }
    if(size.flags & PResizeInc) {
        this->incw_ = size.width_inc;
        this->inch_ = size.height_inc;
    } else {
        this->incw_ = this->inch_ = 0;
    }
    if(size.flags & PMaxSize) {
        this->maxw_ = size.max_width;
        this->maxh_ = size.max_height;
    } else {
        this->maxw_ = this->maxh_ = 0;
    }
    if(size.flags & PMinSize) {
        this->minw_ = size.min_width;
        this->minh_ = size.min_height;
    }
    else if(size.flags & PBaseSize) {
        this->minw_ = size.base_width;
        this->minh_ = size.base_height;
    } else {
        this->minw_ = this->minh_ = 0;
    }
    if(size.flags & PAspect) {
        this->mina_ = (float)size.min_aspect.y / size.min_aspect.x;
        this->maxa_ = (float)size.max_aspect.x / size.max_aspect.y;
    } else {
        this->maxa_ = this->mina_ = 0.0;
    }
    //this->isfixed = (this->maxw && this->minw && this->maxh && this->minh
    //             && this->maxw == this->minw && this->maxh == this->minh);
}

ResizeAction Client::possibleResizeActions()
{
    if (is_client_floated()) {
        ResizeAction act;
        act.left = true;
        act.right = true;
        act.top = true;
        act.bottom = true;
        // floating clients can be resized into all directions
        return act;
    } else {
        ResizeAction act;
        shared_ptr<FrameLeaf> frame = tag_->frame->findFrameWithClient(this);
        vector<pair<Direction, bool*>> directions = {
            { Direction::Left, &(act.left) },
            { Direction::Right, &(act.right) },
            { Direction::Up, &(act.top) },
            { Direction::Down, &(act.bottom) },
        };
        int idx = frame->clientIndex(this);
        for (auto& it : directions) {
            Direction dir = it.first;
            if (frame->getInnerNeighbourIndex(dir, DirectionLevel::Visible, idx) >= 0) {
                // if the client has a neighbour within this frame in the specified
                // direction, then we can't resize.
                *(it.second) = false;
            } else {
                // if the client touches the outside of its frame,
                // we can resize if the frame has a neighbour in the
                // specified direction
                *(it.second) = (bool)frame->neighbour(dir);
            }
        }
        return act;
    }
}




void Client::send_configure(bool force) {
    auto last_inner_rect = dec->last_inner();
    // force is just a quick fix: sometimes it is mandatory
    // to send a configure request even if the geometry didn't change.
    if (content_geometry_ == last_inner_rect && !force) {
        // only send the configure notify if the window geometry really changed.
        // otherwise, sending this might trigger an endless loop between clients
        // and hlwm.
        return;
    }
    content_geometry_ = last_inner_rect;
    XConfigureEvent ce;
    ce.type = ConfigureNotify;
    ce.display = X_.display();
    ce.event = this->window_;
    ce.window = this->window_;
    ce.x = last_inner_rect.x;
    ce.y = last_inner_rect.y;
    ce.width = std::max(last_inner_rect.width, WINDOW_MIN_WIDTH);
    ce.height = std::max(last_inner_rect.height, WINDOW_MIN_HEIGHT);
    ce.border_width = 0;
    ce.above = None;
    ce.override_redirect = False;
    XSendEvent(X_.display(), this->window_, False, StructureNotifyMask, (XEvent *)&ce);
}

void Client::resize_floating(Monitor* m, bool isFocused) {
    if (!m) {
        return;
    }
    Rectangle rect = m->clampRelativeGeometry(float_size_);
    rect.x += m->rect->x;
    rect.y += m->rect->y;
    rect.x += m->pad_left();
    rect.y += m->pad_up();
    *decParams = DecorationParameters();
    decParams->floating_ = true;
    decParams->focused_ = isFocused;
    decParams->urgent_ = urgent_();
    dec->setParameters(*decParams);
    dec->resize_inner(rect);
}

Rectangle Client::outer_floating_rect() {
    return dec->inner_to_outer(float_size_);
}

bool Client::is_client_floated() {
    if (floating_()) {
        return true;
    }
    auto t = tag();
    if (!t) {
        return false;
    } else {
        return tag()->floating;
    }
}

void Client::requestClose() { //! ask the client to close
    ewmh.windowClose(window_);
}

void Client::set_visible(bool visible) {
    if (visible == this->visible_()) {
        return;
    }
    if (visible) {
        /* Grab the server to make sure that the frame window is mapped before
           the client gets its MapNotify, i.e. to make sure the client is
           _visible_ when it gets MapNotify. */
        XGrabServer(X_.display());
        ewmh.windowUpdateWmState(this->window_, WmState::WSNormalState);
        XMapWindow(X_.display(), this->window_);
        if (decorated_()) {
            XMapWindow(X_.display(), this->dec->decorationWindow());
        }
        XUngrabServer(X_.display());
    } else {
        /* we unmap the client itself so that we can get MapRequest
           events, and because the ICCCM tells us to! */
        if (decorated_()) {
            XUnmapWindow(X_.display(), this->dec->decorationWindow());
        }
        XUnmapWindow(X_.display(), this->window_);
        ewmh.windowUpdateWmState(this->window_, WmState::WSIconicState);
        this->ignore_unmaps_++;
    }
    this->visible_ = visible;
}

void Client::urgencyAttributeChanged(bool state)
{
    if (this == manager.focus() && state == true) {
        // suppress it if the focused client wants to be urgent
        urgent_ = false;
        return;
    }
    hook_emit({"urgent", state ? "on" : "off", WindowID(window_).str() });
    decParams->urgent_ = urgent_();
    redraw();
    redrawRelevantTabBars();
    if (state != x11urgent_) {
        x11urgent_ = state;
        X_.setWindowUrgencyHint(window_, state);
    }
    ewmh.updateWindowState(this);
    // report changes to tags
    tag_set_flags_dirty();
}

// heavily inspired by dwm.c
/**
 * @brief read the WM Hints from the X11 window
 * and set member variables x11urgent_ and neverfocus_ accordingly.
 * After that, set urgent_ accordingly.
 * @param forceNotUrgent: If set, then the urgency hint of the window
 * is ignored and even reset to 'not urgent'.
 */
void Client::readWmHints(bool forceNotUrgent) {
    XWMHints* wmh = XGetWMHints(X_.display(), this->window_);
    if (!wmh) {
        x11urgent_ = false;
        neverfocus_ = false;
        return;
    }

    if (forceNotUrgent && (wmh->flags & XUrgencyHint)) {
        // remove urgency hint if window is focused
        wmh->flags &= ~XUrgencyHint;
        XSetWMHints(X_.display(), this->window_, wmh);
        x11urgent_ = false;
    } else {
        // set x11urgent_
        x11urgent_ = wmh->flags & XUrgencyHint;
    }
    if (wmh->flags & InputHint) {
        this->neverfocus_ = !wmh->input;
    } else {
        this->neverfocus_ = false;
    }
    XFree(wmh);
    // make attribute reflect urgency hint:
    urgent_ = x11urgent_;
}

void Client::update_title() {
    string newName = ewmh.getWindowTitle(window_);
    bool changed = title_() != newName;
    title_ = newName;
    if (changed && get_current_client() == this) {
        hook_emit({"window_title_changed", WindowID(window_).str(), title_()});
    }
}

Client* get_current_client() {
    return Root::get()->monitors->focus()->tag->focusedClient();
}

void Client::updateEwmhState() {
    if (ewmhnotify_) {
        ewmhfullscreen_ = fullscreen_();
    }
    ewmh.updateWindowState(this);
}

void Client::floatingGeometryChanged()
{
    Rectangle geom = float_size_();
    if (sizehints_floating_()) {
        applysizehints(&geom.width, &geom.height, true);
        float_size_ = geom;
    }
    if (is_client_floated()) {
        needsRelayout.emit(tag());
    }
}

/**
 * @brief to be called whenever the 'decorated' attribute changed
 */
void Client::fixParentWindow(bool decorated)
{
    if (!decorated) {
        XReparentWindow(X_.display(), window_, X_.root(), 40, 40);
        XUnmapWindow(X_.display(), dec->decorationWindow());
    } else {
        if (visible_()) {
            XMapWindow(X_.display(), dec->decorationWindow());
        }
        XReparentWindow(X_.display(), window_, dec->decorationWindow(), 40, 40);
    }
    // the unmap triggers an unmap notify for the window itself
    ignore_unmaps_++;
    needsRelayout.emit(this->tag());
}

Rectangle Client::decorationGeometry()
{
    return dec->last_outer();
}

string Client::getWindowClass()
{
    return ewmh.X().getClass(window_);
}

string Client::getWindowInstance()
{
    return ewmh.X().getInstance(window_);
}

FrameLeaf* Client::parentFrame()
{
    if (is_client_floated()) {
        return nullptr;
    } else {
        if (!tag_) {
            // a fallback if this is called in the early
            // phase of a client object
            return nullptr;
        }
        return tag_->frame->findFrameWithClient(this).get();
    }
}

void Client::requestRedraw()
{
    if (tag_) {
        needsRelayout.emit(tag_);
    }
}

void Client::fuzzy_fix_initial_position() {
    // find out the top-left-most position of the decoration,
    // considering the current settings of possible floating decorations
    int extreme_x = float_size_->x;
    int extreme_y = float_size_->y;
    if (decorated_()) {
        // fake some decoration parameters for determining
        // the size of the decoration
        DecorationParameters fakeDecParams;
        // compute the sizes of all the widgets in the decoration
        fakeDecParams.floating_ = true;
        fakeDecParams.focused_ = true;
        dec->setParameters(fakeDecParams);
        dec->computeWidgetGeometries(float_size_);
        auto r = dec->inner_to_outer(float_size_);
        extreme_x = std::min(extreme_x, r.x);
        extreme_y = std::min(extreme_y, r.y);

        fakeDecParams.focused_ = false;
        dec->setParameters(fakeDecParams);
        dec->computeWidgetGeometries(float_size_);
        r = dec->inner_to_outer(float_size_);
        extreme_x = std::min(extreme_x, r.x);
        extreme_y = std::min(extreme_y, r.y);
    }
    // if top left corner might be outside of the monitor, move it accordingly
    Point2D delta = {0, 0};
    if (extreme_x < 0) { delta.x = abs(extreme_x); }
    if (extreme_y < 0) { delta.y = abs(extreme_y); }
    if (delta.x || delta.y) {
        float_size_ = float_size_->shifted(delta);
    }
}

void Client::clear_properties() {
    ewmh.clearClientProperties(window_);
}

//! name of the tag on which the client is
string Client::tagName() {
    // be safe during initialization phase and don't assume
    // that tag is set.
    return tag_ ? tag_->name() : "";
}

Window Client::decorationWindow() {
    return dec->decorationWindow();
}

