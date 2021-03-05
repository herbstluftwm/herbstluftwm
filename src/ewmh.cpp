#include "ewmh.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <algorithm>
#include <cstdio>

#include "client.h"
#include "globals.h"
#include "hlwmcommon.h"
#include "layout.h"
#include "monitor.h"
#include "monitormanager.h"
#include "mousemanager.h"
#include "root.h"
#include "settings.h"
#include "stack.h"
#include "tagmanager.h"
#include "utils.h"
#include "xconnection.h"

using std::pair;
using std::string;
using std::vector;

/* list of names of all _NET-atoms */
const std::array<const char*,NetCOUNT> Ewmh::netatomNames_ =
  ArrayInitializer<const char*,NetCOUNT>({
    { NetSupported                   , "_NET_SUPPORTED"                    },
    { NetClientList                  , "_NET_CLIENT_LIST"                  },
    { NetClientListStacking          , "_NET_CLIENT_LIST_STACKING"         },
    { NetCloseWindow                 , "_NET_CLOSE_WINDOW"                 },
    { NetNumberOfDesktops            , "_NET_NUMBER_OF_DESKTOPS"           },
    { NetCurrentDesktop              , "_NET_CURRENT_DESKTOP"              },
    { NetDesktopNames                , "_NET_DESKTOP_NAMES"                },
    { NetWmDesktop                   , "_NET_WM_DESKTOP"                   },
    { NetDesktopViewport             , "_NET_DESKTOP_VIEWPORT"             },
    { NetActiveWindow                , "_NET_ACTIVE_WINDOW"                },
    { NetWmName                      , "_NET_WM_NAME"                      },
    { NetSupportingWmCheck           , "_NET_SUPPORTING_WM_CHECK"          },
    { NetWmWindowType                , "_NET_WM_WINDOW_TYPE"               },
    { NetWmState                     , "_NET_WM_STATE"                     },
    { NetWmWindowOpacity             , "_NET_WM_WINDOW_OPACITY"            },
    { NetMoveresizeWindow            , "_NET_MOVERESIZE_WINDOW"            },
    { NetWmMoveresize                , "_NET_WM_MOVERESIZE"                },
    { NetFrameExtents                , "_NET_FRAME_EXTENTS"                },
    /* window states */
    { NetWmStateFullscreen           , "_NET_WM_STATE_FULLSCREEN"          },
    { NetWmStateHidden               , "_NET_WM_STATE_HIDDEN"              },
    { NetWmStateDemandsAttention     , "_NET_WM_STATE_DEMANDS_ATTENTION"   },
    /* window types */
    { NetWmWindowTypeDesktop         , "_NET_WM_WINDOW_TYPE_DESKTOP"       },
    { NetWmWindowTypeDock            , "_NET_WM_WINDOW_TYPE_DOCK"          },
    { NetWmWindowTypeToolbar         , "_NET_WM_WINDOW_TYPE_TOOLBAR"       },
    { NetWmWindowTypeMenu            , "_NET_WM_WINDOW_TYPE_MENU"          },
    { NetWmWindowTypeUtility         , "_NET_WM_WINDOW_TYPE_UTILITY"       },
    { NetWmWindowTypeSplash          , "_NET_WM_WINDOW_TYPE_SPLASH"        },
    { NetWmWindowTypeDialog          , "_NET_WM_WINDOW_TYPE_DIALOG"        },
    { NetWmWindowTypeDropdownMenu    , "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU" },
    { NetWmWindowTypePopupMenu       , "_NET_WM_WINDOW_TYPE_POPUP_MENU"    },
    { NetWmWindowTypeTooltip         , "_NET_WM_WINDOW_TYPE_TOOLTIP"       },
    { NetWmWindowTypeNotification    , "_NET_WM_WINDOW_TYPE_NOTIFICATION"  },
    { NetWmWindowTypeCombo           , "_NET_WM_WINDOW_TYPE_COMBO"         },
    { NetWmWindowTypeDnd             , "_NET_WM_WINDOW_TYPE_DND"           },
    { NetWmWindowTypeNormal          , "_NET_WM_WINDOW_TYPE_NORMAL"        },
}).a;

Ewmh::Ewmh(XConnection& xconnection)
    : X_(xconnection)
{
    /* init ewmh net atoms */
    for (int i = 0; i < NetCOUNT; i++) {
        if (!netatomNames_[i]) {
            HSWarning("no name specified in g_netatom_names "
                      "for atom number %d\n", i);
            continue;
        }
        netatom_[i] = XInternAtom(X_.display(), netatomNames_[i], False);
    }

    vector<pair<WM,const char*>> wm2name = {
        { WM::Name,         "WM_NAME" },
        { WM::Protocols,    "WM_PROTOCOLS" },
        { WM::Delete,       "WM_DELETE_WINDOW" },
        { WM::State,        "WM_STATE" },
        { WM::ChangeState,  "WM_CHANGE_STATE" },
        { WM::TakeFocus,    "WM_TAKE_FOCUS" },
    };
    for (const auto& init : wm2name) {
        auto atom = XInternAtom(X_.display(), init.second, False);
        wmatom_[static_cast<size_t>(init.first)] = atom;
    }

    readInitialEwmhState();

    /* init for the supporting wm check */
    windowManagerWindow_ = XCreateSimpleWindow(X_.display(), X_.root(),
                                      -100, -100, 1, 1, 0, 0, CWOverrideRedirect | CWEventMask);
    X_.setPropertyWindow(windowManagerWindow_, netatom_[NetSupportingWmCheck], { windowManagerWindow_ });
    XMapWindow(X_.display(), windowManagerWindow_);
}

/**
 * @brief Fully take ownership of the window management.
 */
void Ewmh::installWmWindow()
{
    /* tell which ewmh atoms are supported */
    XChangeProperty(X_.display(), X_.root(), netatom_[NetSupported], XA_ATOM, 32,
        PropModeReplace, (unsigned char *) netatom_, NetCOUNT);
    X_.setPropertyWindow(X_.root(), netatom_[NetSupportingWmCheck], { windowManagerWindow_ });

    /* init atoms that never change */
    X_.setPropertyCardinal(X_.root(), netatom_[NetDesktopViewport], {0, 0});
}


//! read the current ewmh properties from the root window
void Ewmh::readInitialEwmhState()
{
    // list of desktops
    auto number = X_.getWindowPropertyCardinal(X_.root(), netatom_[NetNumberOfDesktops]);
    if (number.has_value() && !number.value().empty()) {
        auto val = number.value()[0];
        initialState_.numberOfDesktops = (val >= 0) ? ((size_t)(val)) : 0;
    }
    auto maybe_names = X_.getWindowPropertyTextList(X_.root(), netatom_[NetDesktopNames]);
    if (maybe_names.has_value()) {
        initialState_.desktopNames = maybe_names.value();
    }
    // list of managed clients
    auto maybe_clients =
        X_.getWindowPropertyWindow(X_.root(), netatom_[NetClientList]);
    initialState_.original_client_list_ =
        maybe_clients.has_value() ? maybe_clients.value() : vector<Window>();
    if (g_verbose) {
        initialState_.print(stderr);
    }
}

long Ewmh::windowGetInitialDesktop(Window win)
{
    auto maybe_idx = X_.getWindowPropertyCardinal(win, netatom_[NetWmDesktop]);
    if (maybe_idx.has_value() && !maybe_idx.value().empty()) {
        return maybe_idx.value()[0];
    }
    return -1;
}

void Ewmh::InitialState::print(FILE *file)
{
    fprintf(file, "EWMH: %zu desktops:", numberOfDesktops);
    for (const auto& n : desktopNames) {
        fprintf(file, " \'%s\'", n.c_str());
    }
    fprintf(file, "\n");
    fprintf(file, "%zu managed clients: ", original_client_list_.size());
    for (auto win : original_client_list_) {
        fprintf(file, "  window 0x%lx", win);
    }
    fprintf(file, "\n");
}


void Ewmh::injectDependencies(Root* root) {
    root_ = root;
    tags_ = root->tags();
}

void Ewmh::updateAll() {
    /* init many properties */
    updateWmName();
    updateClientList();
    updateClientListStacking();
    updateDesktops();
    updateCurrentDesktop();
    updateDesktopNames();
}

Ewmh::~Ewmh() {
    XDeleteProperty(X_.display(), X_.root(), netatom_[NetSupportingWmCheck]);
    XDestroyWindow(X_.display(), windowManagerWindow_);
}

void Ewmh::updateWmName() {
    string name = root_->settings->wmname();
    X_.setPropertyString(windowManagerWindow_, netatom_[NetWmName], name);
    X_.setPropertyString(X_.root(), netatom_[NetWmName], name);
}

void Ewmh::updateClientList() {
    X_.setPropertyWindow(X_.root(), netatom_[NetClientList], netClientList_);
}

const Ewmh::InitialState &Ewmh::initialState()
{
    return initialState_;
}

void Ewmh::updateClientListStacking() {
    // First: get the windows currently visible
    vector<Window> buf;
    auto addToVector = [&buf](Window w) { buf.push_back(w); };
    g_monitors->extractWindowStack(true, addToVector);

    // Then add all the invisible windows at the end
    for (auto tag : *tags_) {
        if (find_monitor_with_tag(tag)) {
        // do not add tags because they are already added
            continue;
        }
        tag->stack->extractWindows(true, addToVector);
    }

    // reverse stacking order, because ewmh requires bottom to top order
    std::reverse(buf.begin(), buf.end());

    X_.setPropertyWindow(X_.root(), netatom_[NetClientListStacking], buf);
}

void Ewmh::addClient(Window win) {
    netClientList_.push_back(win);
    updateClientList();
    updateClientListStacking();
}

void Ewmh::removeClient(Window win) {
    netClientList_.erase(std::remove(netClientList_.begin(), netClientList_.end(), win), netClientList_.end());
    updateClientList();
    updateClientListStacking();
}

void Ewmh::updateDesktops() {
    X_.setPropertyCardinal(X_.root(), netatom_[NetNumberOfDesktops],
                           { (long) root_->tags->size() });
}

void Ewmh::updateDesktopNames() {
    vector<string> names;
    for (auto tag : *tags_) {
        names.push_back(tag->name);
    }
    X_.setPropertyString(X_.root(), netatom_[NetDesktopNames], names);
}

void Ewmh::updateCurrentDesktop() {
    HSTag* tag = get_current_monitor()->tag;
    int index = tags_->index_of(tag);
    if (index < 0) {
        HSWarning("tag %s not found in internal list\n", tag->name->c_str());
        return;
    }
    X_.setPropertyCardinal(X_.root(), netatom_[NetCurrentDesktop], { index });
}

void Ewmh::windowUpdateTag(Window win, HSTag* tag) {
    if (!tag) {
        return;
    }
    int index = tag->index();
    X_.setPropertyCardinal(win, netatom_[NetWmDesktop], { index });
}

void Ewmh::updateActiveWindow(Window win) {
    X_.setPropertyWindow(X_.root(), netatom_[NetActiveWindow], { win });
}

bool Ewmh::focusStealingAllowed(long source) {
    if (root_->settings->focus_stealing_prevention()) {
        /* only allow it to pagers/taskbars */
        return (source == 2);
    } else {
        /* no prevention */
        return true;
    }
}

void Ewmh::handleClientMessage(XClientMessageEvent* me) {
    HSDebug("Received event: ClientMessage: \"%s\" for %lx\n",
            X_.atomName(me->message_type).c_str(),
            me->window);
    if (me->message_type == wmatom(WM::ChangeState)) {
        if (me->data.l[0] == static_cast<long>(WmState::WSIconicState)) {
            Client* client = Root::common().client(me->window);
            if (client) {
                client->minimized_ = true;
            }
        }
        return;
    }
    int index;
    for (index = 0; index < NetCOUNT; index++) {
        if (me->message_type == netatom_[index]) {
            break;
        }
    }
    if (index >= NetCOUNT) {
        HSDebug("received unknown client message\n");
        return;
    }

    int desktop_index;
    switch (index) {
        case NetActiveWindow: {
            // only steal focus if allowed to the current source
            // (i.e. me->data.l[0] in this case as specified by EWMH)
            if (focusStealingAllowed(me->data.l[0])) {
                auto client = Root::common().client(me->window);
                if (client) {
                    focus_client(client, true, true, true);
                }
            } else {
                // Focus stealing is not allowed, at least mark the client urgent
                auto client = Root::common().client(me->window);
                if (client) {
                    client->set_urgent(true);
                }
            }
            break;
        }

        case NetCurrentDesktop: {
            desktop_index = me->data.l[0];
            if (desktop_index < 0 || desktop_index >= tag_get_count()) {
                HSDebug("_NET_CURRENT_DESKTOP: invalid index \"%d\"\n",
                        desktop_index);
                break;
            }
            HSTag* tag = get_tag_by_index(desktop_index);
            monitor_set_tag(get_current_monitor(), tag);
            break;
        }

        case NetWmDesktop: {
            desktop_index = me->data.l[0];
            if (!focusStealingAllowed(me->data.l[1])) {
                break;
            }
            HSTag* target = get_tag_by_index(desktop_index);
            auto client = Root::common().client(me->window);
            if (client && target) {
                tags_->moveClient(client, target);
            }
            break;
        }

        case NetWmState: {
            auto client = Root::common().client(me->window);
            /* ignore requests for unmanaged windows */
            if (!client || !client->ewmhrequests_) {
                break;
            }

            /* mapping between EWMH atoms and client struct members */
            struct {
                int     atom_index;
                bool    enabled;
                void    (*callback)(Client*, bool);
            } client_atoms[] = {
                { NetWmStateFullscreen,
                    client->fullscreen_,     [](Client* c, bool state){ c->fullscreen_ = state; } },
                { NetWmStateDemandsAttention,
                    client->urgent_,         [](Client* c, bool state){ c->set_urgent(state); } },
            };

            /* me->data.l[1] and [2] describe the properties to alter */
            for (int prop = 1; prop <= 2; prop++) {
                if (me->data.l[prop] == 0) {
                    /* skip if no property is specified */
                    continue;
                }
                /* check if we support the property data[prop] */
                size_t i;
                for (i = 0; i < LENGTH(client_atoms); i++) {
                    if (netatom_[client_atoms[i].atom_index]
                        == static_cast<unsigned int>(me->data.l[prop])) {
                        break;
                    }
                }
                if (i >= LENGTH(client_atoms)) {
                    /* property will not be handled */
                    continue;
                }
                auto new_value = ArrayInitializer<bool,3>({
                    { _NET_WM_STATE_REMOVE  , false },
                    { _NET_WM_STATE_ADD     , true },
                    { _NET_WM_STATE_TOGGLE  , !client_atoms[i].enabled },
                }).a;
                int action = me->data.l[0];
                if (action >= static_cast<int>(new_value.size())) {
                    HSDebug("_NET_WM_STATE: invalid action %d\n", action);
                }
                /* change the value */
                client_atoms[i].callback(client, new_value[action]);
            }
            break;
        }

        case NetWmMoveresize: {
            auto client = Root::common().client(me->window);
            if (!client) {
                break;
            }
            int direction = me->data.l[2];
            if (direction == _NET_WM_MOVERESIZE_MOVE
                || direction == _NET_WM_MOVERESIZE_MOVE_KEYBOARD) {
                root_->mouse->mouse_initiate_move(client, {});
            } else if (direction == _NET_WM_MOVERESIZE_CANCEL) {
                if (root_->mouse->mouse_is_dragging()) {
                    root_->mouse->mouse_stop_drag();
                }
            } else {
                // anything else is a resize
                root_->mouse->mouse_initiate_resize(client, {});
            }
            break;
        }

        case NetCloseWindow: {
            windowClose(me->window);
            break;
        }

        default:
            HSDebug("no handler for the client message \"%s\"\n",
                    netatomNames_[index]);
            break;
    }
}

void Ewmh::updateWindowState(Client* client) {
    /* mapping between EWMH atoms and client struct members */
    struct {
        int     atom_index;
        bool    enabled;
    } client_atoms[] = {
        { NetWmStateFullscreen,         client->ewmhfullscreen_  },
        { NetWmStateDemandsAttention,   client->urgent_          },
        { NetWmStateHidden,             client->minimized_       },
    };

    /* find out which flags are set */
    Atom window_state[LENGTH(client_atoms)];
    size_t count_enabled = 0;
    for (size_t i = 0; i < LENGTH(client_atoms); i++) {
        if (client_atoms[i].enabled) {
            window_state[count_enabled] = netatom_[client_atoms[i].atom_index];
            count_enabled++;
        }
    }

    /* write it to the window */
    XChangeProperty(X_.display(), client->window_, netatom_[NetWmState], XA_ATOM,
        32, PropModeReplace, (unsigned char *) window_state, count_enabled);
}

void Ewmh::clearClientProperties(Window win) {
    // delete ewmh-properties and ICCCM-Properties such that the client knows
    // that he has been unmanaged and now the client is allowed to be mapped
    // again (e.g. if it is some dialog)
    XDeleteProperty(X_.display(), win, netatom_[NetWmState]);
    XDeleteProperty(X_.display(), win, wmatom(WM::State));
}

bool Ewmh::isWindowStateSet(Window win, Atom hint) {
    auto res = X_.getWindowPropertyAtom(win, netatom_[NetWmState]);
    if (!res.has_value()) {
        return false;
    }
    for (auto& h : res.value()) {
        if (hint == h) {
            return true;
        }
    }
    return false;
}

bool Ewmh::isFullscreenSet(Window win) {
    return isWindowStateSet(win, netatom_[NetWmStateFullscreen]);
}

void Ewmh::setWindowOpacity(Window win, double opacity) {
    /* Based on the EWMH proposal
     * https://mail.gnome.org/archives/wm-spec-list/2003-December/msg00035.html
     */
    long long_opacity = 0xffffffff * CLAMP(opacity, 0, 1);
    X_.setPropertyCardinal(win, netatom_[NetWmWindowOpacity], { long_opacity });
}

void Ewmh::updateFrameExtents(Window win, int left, int right, int top, int bottom) {
    X_.setPropertyCardinal(win, netatom_[NetFrameExtents],
                           { left, right, top, bottom });
}

void Ewmh::windowUpdateWmState(Window win, WmState state) {
    /* set full WM_STATE according to
     * http://www.x.org/releases/X11R7.7/doc/xorg-docs/icccm/icccm.html#WM_STATE_Property
     *
     * It's crucial that the property has the type WM_STATE!
     */
    long wmstate[] = { static_cast<long>(state), None };
    XChangeProperty(X_.display(), win,  wmatom(WM::State), wmatom(WM::State),
                    32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(wmstate), LENGTH(wmstate));
}

bool Ewmh::isOwnWindow(Window win) {
    return windowManagerWindow_ == win;
}

void Ewmh::clearInputFocus() {
    XSetInputFocus(X_.display(), windowManagerWindow_, RevertToPointerRoot, CurrentTime);
}

Ewmh& Ewmh::get() {
    return Root::get()->ewmh_;
}

/** send the given proto atom to the given window via XSendEvent(). If
 * checkProtocols = true, this is done only if proto is present in the window's
 * WM protocols. The return value tells whether the event was actually sent.
 */
bool Ewmh::sendEvent(Window window, Ewmh::WM proto, bool checkProtocols) {
    bool exists = false;
    Atom protoAtom = wmatom(proto);
    if (!checkProtocols) {
        exists = true;
    } else {
        int n;
        Atom *protocols;

        if (XGetWMProtocols(X_.display(), window, &protocols, &n)) {
            while (!exists && n--) {
                exists = protocols[n] == protoAtom;
            }
            XFree(protocols);
        }
    }
    if (exists) {
        XEvent ev;
        ev.type = ClientMessage;
        ev.xclient.window = window;
        ev.xclient.message_type = wmatom(WM::Protocols);
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = protoAtom;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(X_.display(), window, False, NoEventMask, &ev);
    }
    return exists;
}

void Ewmh::windowClose(Window window) {
    sendEvent(window, WM::Delete, false);
}

Atom Ewmh::netatom(int netatomEnum)
{
    return netatom_[netatomEnum];
}

const char* Ewmh::netatomName(int netatomEnum)
{
    return netatomNames_[static_cast<unsigned long>(netatomEnum)];
}

//! convenience wrapper around wmatom_
Atom Ewmh::wmatom(WM proto) {
    return wmatom_[(int)proto];
}

string Ewmh::getWindowTitle(Window win) {
    auto newName = X_.getWindowProperty(win, netatom_[NetWmName]);
    if (newName.has_value()) {
        return newName.value();
    }
    newName = X_.getWindowProperty(win, wmatom(WM::Name));
    if (newName.has_value()) {
        return newName.value();
    }
    return "";
}

/** Return the window type of the given window. If there are multiple entries, then
 * only the first window type entry is returned. The return value is an enum value between
 * NetWmWindowTypeFIRST and NetWmWindowTypeLAST (inclusive). Any other window
 * type is not recognized and leads to -1 being returned.
 */
int Ewmh::getWindowType(Window win) {
    auto atoms = X_.getWindowPropertyAtom(win, netatom_[NetWmWindowType]);
    if (!atoms.has_value() || atoms.value().empty()) {
        return -1;
    }
    Atom windowtype = atoms.value()[0];
    for (int i = NetWmWindowTypeFIRST; i <= NetWmWindowTypeLAST; i++) {
        // try to find the window type
        if (windowtype == netatom_[i]) {
            return i;
        }
    }
    return -1;
}
