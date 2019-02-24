#ifndef __HERBSTLUFT_EWMH_H_
#define __HERBSTLUFT_EWMH_H_

#include <X11/X.h>
#include <X11/Xlib.h>
#include <array>

#define ENUM_WITH_ALIAS(Identifier, Alias) \
    Identifier, Alias = Identifier

/* actions on NetWmState */
#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

class Root;

enum {
    NetSupported = 0,
    NetClientList,
    NetClientListStacking,
    NetNumberOfDesktops,
    NetCurrentDesktop,
    NetDesktopNames,
    NetWmDesktop,
    NetDesktopViewport,
    NetActiveWindow,
    NetWmName,
    NetSupportingWmCheck,
    NetWmWindowType,
    NetWmState,
    NetWmWindowOpacity,
    NetMoveresizeWindow,
    NetWmMoveresize,
    NetFrameExtents,
    /* window states */
    NetWmStateFullscreen,
    NetWmStateDemandsAttention,
    /* window types */
    ENUM_WITH_ALIAS(NetWmWindowTypeDesktop, NetWmWindowTypeFIRST),
    NetWmWindowTypeDock,
    NetWmWindowTypeToolbar,
    NetWmWindowTypeMenu,
    NetWmWindowTypeUtility,
    NetWmWindowTypeSplash,
    NetWmWindowTypeDialog,
    NetWmWindowTypeDropdownMenu,
    NetWmWindowTypePopupMenu,
    NetWmWindowTypeTooltip,
    NetWmWindowTypeNotification,
    NetWmWindowTypeCombo,
    NetWmWindowTypeDnd,
    ENUM_WITH_ALIAS(NetWmWindowTypeNormal, NetWmWindowTypeLAST),
    /* the count of hints */
    NetCOUNT
};

// possible values for direction of a _NET_WM_MOVERESIZE client message
#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT      0
#define _NET_WM_MOVERESIZE_SIZE_TOP          1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT        3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM       5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define _NET_WM_MOVERESIZE_SIZE_LEFT         7
#define _NET_WM_MOVERESIZE_MOVE              8   /* movement only */
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD     9   /* size via keyboard */
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD    10   /* move via keyboard */
#define _NET_WM_MOVERESIZE_CANCEL           11   /* cancel operation */

class HSTag;
class Client;

extern Atom g_netatom[NetCOUNT];

extern const std::array<const char*,NetCOUNT> g_netatom_names;

void ewmh_init();
void ewmh_destroy();
void ewmh_update_all();

void ewmh_add_client(Window win);
void ewmh_remove_client(Window win);
void ewmh_set_wmname(const char* name);
void ewmh_update_wmname();

void ewmh_update_client_list();
void ewmh_get_original_client_list(Window** buf, unsigned long *count);
void ewmh_update_client_list_stacking();
void ewmh_update_desktops();
void ewmh_update_desktop_names();
void ewmh_update_active_window(Window win);
void ewmh_update_current_desktop();
void ewmh_update_window_state(Client* client);
void ewmh_update_frame_extents(Window win, int left, int right, int top, int bottom);
bool ewmh_is_window_state_set(Window win, Atom hint);
bool ewmh_is_fullscreen_set(Window win);
void ewmh_clear_client_properties(Window win);

// set the desktop property of a window
void ewmh_window_update_tag(Window win, HSTag* tag);

void ewmh_handle_client_message(Root* root, XEvent* event);

void ewmh_set_window_opacity(Window win, double opacity);

typedef enum {
    // see icccm:
    // http://www.x.org/releases/X11R7.7/doc/xorg-docs/icccm/icccm.html#WM_STATE_Property
    WmStateWithdrawnState = 0,
    WmStateNormalState    = 1,
    WmStateIconicState    = 3,
} WmState;

void window_update_wm_state(Window win, WmState state);

#endif

