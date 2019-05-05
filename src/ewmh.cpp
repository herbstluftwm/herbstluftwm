#include "ewmh.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <algorithm>
#include <cstring>
#include <limits>

#include "client.h"
#include "layout.h"
#include "monitor.h"
#include "monitormanager.h"
#include "mouse.h"
#include "root.h"
#include "settings.h"
#include "stack.h"
#include "tagmanager.h"
#include "utils.h"
#include "xconnection.h"

using std::function;
using std::make_shared;
using std::vector;

Atom g_netatom[NetCOUNT];

// module internal globals:
static vector<Window> g_windows; // array with Window-IDs in initial mapping order
static Window      g_wm_window;

static int WM_STATE;

static Window*  g_original_clients = nullptr;
static unsigned long g_original_clients_count = 0;

/* list of names of all _NET-atoms */
const std::array<const char*,NetCOUNT>g_netatom_names =
  ArrayInitializer<const char*,NetCOUNT>({
    { NetSupported                   , "_NET_SUPPORTED"                    },
    { NetClientList                  , "_NET_CLIENT_LIST"                  },
    { NetClientListStacking          , "_NET_CLIENT_LIST_STACKING"         },
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
        if (!g_netatom_names[i]) {
            HSWarning("no name specified in g_netatom_names "
                      "for atom number %d\n", i);
            continue;
        }
        g_netatom[i] = XInternAtom(X_.display(), g_netatom_names[i], False);
    }

    /* tell which ewmh atoms are supported */
    XChangeProperty(X_.display(), X_.root(), g_netatom[NetSupported], XA_ATOM, 32,
        PropModeReplace, (unsigned char *) g_netatom, NetCOUNT);

    /* init some globals */
    if (!readClientList(&g_original_clients, &g_original_clients_count))
    {
        g_original_clients = nullptr;
        g_original_clients_count = 0;
    }

    /* init other atoms */
    WM_STATE = XInternAtom(X_.display(), "WM_STATE", False);

    /* init for the supporting wm check */
    g_wm_window = XCreateSimpleWindow(X_.display(), X_.root(),
                                      42, 42, 42, 42, 0, 0, 0);
    X_.setPropertyWindow(X_.root(), g_netatom[NetSupportingWmCheck], { g_wm_window });
    X_.setPropertyWindow(g_wm_window, g_netatom[NetSupportingWmCheck], { g_wm_window });

    /* init atoms that never change */
    X_.setPropertyCardinal(X_.root(), g_netatom[NetDesktopViewport], {0, 0});
}

void Ewmh::injectDependencies(Root* root) {
    root_ = root;
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
    if (g_original_clients) {
        XFree(g_original_clients);
    }
    XDeleteProperty(X_.display(), X_.root(), g_netatom[NetSupportingWmCheck]);
    XDestroyWindow(X_.display(), g_wm_window);
}

void Ewmh::setWmName(const char* name) {
    XChangeProperty(X_.display(), g_wm_window, g_netatom[NetWmName],
        ATOM("UTF8_STRING"), 8, PropModeReplace,
        (unsigned char*)name, strlen(name));
    XChangeProperty(X_.display(), X_.root(), g_netatom[NetWmName],
        ATOM("UTF8_STRING"), 8, PropModeReplace,
        (unsigned char*)name, strlen(name));
}

void Ewmh::updateWmName() {
    setWmName(root_->settings->wmname().c_str());
}

void Ewmh::updateClientList() {
    X_.setPropertyWindow(X_.root(), g_netatom[NetClientList], g_windows);
}

bool Ewmh::readClientList(Window** buf, unsigned long *count) {
    Atom actual_type;
    int format;
    unsigned long bytes_left;
    if (Success != XGetWindowProperty(X_.display(), X_.root(),
            g_netatom[NetClientList], 0, ~0L, False, XA_WINDOW, &actual_type,
            &format, count, &bytes_left, (unsigned char**)buf)) {
        return false;
    }
    if (bytes_left || actual_type != XA_WINDOW || format != 32) {
        return false;
    }
    return true;
}

void Ewmh::getOriginalClientList(Window** buf, unsigned long *count) {
    *buf = g_original_clients;
    *count = g_original_clients_count;
}

void Ewmh::updateClientListStacking() {
    // First: get the windows currently visible
    vector<Window> buf;
    auto addToVector = [&buf](Window w) { buf.push_back(w); };
    g_monitors->extractWindowStack(true, addToVector);

    // Then add all the invisible windows at the end
    for (auto tag : *global_tags) {
        if (find_monitor_with_tag(tag)) {
        // do not add tags because they are already added
            continue;
        }
        tag->stack->extractWindows(true, addToVector);
    }

    // reverse stacking order, because ewmh requires bottom to top order
    std::reverse(buf.begin(), buf.end());

    X_.setPropertyWindow(X_.root(), g_netatom[NetClientListStacking], buf);
}

void Ewmh::addClient(Window win) {
    g_windows.push_back(win);
    updateClientList();
    updateClientListStacking();
}

void Ewmh::removeClient(Window win) {
    g_windows.erase(std::remove(g_windows.begin(), g_windows.end(), win), g_windows.end());
    updateClientList();
    updateClientListStacking();
}

void Ewmh::updateDesktops() {
    X_.setPropertyCardinal(X_.root(), g_netatom[NetNumberOfDesktops],
                           { (long) root_->tags->size() });
}

void Ewmh::updateDesktopNames() {
    // we know that the tags don't change during the following lines
    vector<const char*> names;
    for (auto tag : *global_tags) {
        names.push_back(tag->name->c_str());
    }
    XTextProperty text_prop;
    Xutf8TextListToTextProperty(X_.display(), (char**)names.data(), names.size(),
                                XUTF8StringStyle, &text_prop);
    XSetTextProperty(X_.display(), X_.root(), &text_prop, g_netatom[NetDesktopNames]);
    XFree(text_prop.value);
}

void Ewmh::updateCurrentDesktop() {
    HSTag* tag = get_current_monitor()->tag;
    int index = global_tags->index_of(tag);
    if (index < 0) {
        HSWarning("tag %s not found in internal list\n", tag->name->c_str());
        return;
    }
    X_.setPropertyCardinal(X_.root(), g_netatom[NetCurrentDesktop], { index });
}

void Ewmh::windowUpdateTag(Window win, HSTag* tag) {
    int index = global_tags->index_of(tag);
    if (index < 0) {
        HSWarning("tag %s not found in internal list\n", tag->name->c_str());
        return;
    }
    X_.setPropertyCardinal(win, g_netatom[NetWmDesktop], { index });
}

void Ewmh::updateActiveWindow(Window win) {
    X_.setPropertyWindow(X_.root(), g_netatom[NetActiveWindow], { win });
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

void Ewmh::handleClientMessage(XEvent* event) {
    HSDebug("Received event: ClientMessage\n");
    XClientMessageEvent* me = &(event->xclient);
    int index;
    for (index = 0; index < NetCOUNT; index++) {
        if (me->message_type == g_netatom[index]) {
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
            // only steal focus it allowed to the current source
            // (i.e.  me->data.l[0] in this case as specified by EWMH)
            if (focusStealingAllowed(me->data.l[0])) {
                auto client = get_client_from_window(me->window);
                if (client) {
                    focus_client(client, true, true);
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
            auto client = get_client_from_window(me->window);
            if (client && target) {
                global_tags->moveClient(client, target);
            }
            break;
        }

        case NetWmState: {
            auto client = get_client_from_window(me->window);
            /* ignore requests for unmanaged windows */
            if (!client || !client->ewmhrequests_) break;

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
                int i;
                for (i = 0; i < LENGTH(client_atoms); i++) {
                    if (g_netatom[client_atoms[i].atom_index]
                        == me->data.l[prop]) {
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
                if (action >= new_value.size()) {
                    HSDebug("_NET_WM_STATE: invalid action %d\n", action);
                }
                /* change the value */
                client_atoms[i].callback(client, new_value[action]);
            }
            break;
        }

        case NetWmMoveresize: {
            auto client = get_client_from_window(me->window);
            if (!client) {
                break;
            }
            int direction = me->data.l[2];
            if (direction == _NET_WM_MOVERESIZE_MOVE
                || direction == _NET_WM_MOVERESIZE_MOVE_KEYBOARD) {
                mouse_initiate_move(client, {});
            } else if (direction == _NET_WM_MOVERESIZE_CANCEL) {
                if (mouse_is_dragging()) mouse_stop_drag();
            } else {
                // anything else is a resize
                mouse_initiate_resize(client, {});
            }
            break;
        }

        default:
            HSDebug("no handler for the client message \"%s\"\n",
                    g_netatom_names[index]);
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
    };

    /* find out which flags are set */
    Atom window_state[LENGTH(client_atoms)];
    size_t count_enabled = 0;
    for (int i = 0; i < LENGTH(client_atoms); i++) {
        if (client_atoms[i].enabled) {
            window_state[count_enabled] = g_netatom[client_atoms[i].atom_index];
            count_enabled++;
        }
    }

    /* write it to the window */
    XChangeProperty(X_.display(), client->window_, g_netatom[NetWmState], XA_ATOM,
        32, PropModeReplace, (unsigned char *) window_state, count_enabled);
}

void Ewmh::clearClientProperties(Window win) {
    XDeleteProperty(X_.display(), win, g_netatom[NetWmState]);
}

bool Ewmh::isWindowStateSet(Window win, Atom hint) {
    Atom* states;
    Atom actual_type;
    int format;
    unsigned long actual_count, bytes_left;
    if (Success != XGetWindowProperty(X_.display(), win, g_netatom[NetWmState], 0,
            ~0L, False, XA_ATOM, &actual_type, &format, &actual_count,
            &bytes_left, (unsigned char**)&states)) {
        // NetWmState just is not set properly
        return false;
    }
    if (actual_type != XA_ATOM || format != 32 || !states) {
        // invalid format or no entries
        return false;
    }
    bool hint_set = false;
    for (int i = 0; i < actual_count; i++) {
        if (states[i] == hint) {
            hint_set = true;
            break;
        }
    }
    XFree(states);
    return hint_set;
}

bool Ewmh::isFullscreenSet(Window win) {
    return isWindowStateSet(win, g_netatom[NetWmStateFullscreen]);
}

void Ewmh::setWindowOpacity(Window win, double opacity) {
    uint32_t int_opacity = std::numeric_limits<uint32_t>::max()
                            * CLAMP(opacity, 0, 1);

    X_.setPropertyCardinal(win, g_netatom[NetWmWindowOpacity], { int_opacity });
}

void Ewmh::updateFrameExtents(Window win, int left, int right, int top, int bottom) {
    X_.setPropertyCardinal(win, g_netatom[NetFrameExtents],
                           { left, right, top, bottom });
}

void Ewmh::windowUpdateWmState(Window win, WmState state) {
    X_.setPropertyCardinal(win, WM_STATE, { state });
}

Ewmh& Ewmh::get() {
    return *(Root::get()->ewmh);
}
