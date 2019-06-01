#ifndef __HERBSTLUFT_EWMH_H_
#define __HERBSTLUFT_EWMH_H_

#include <X11/X.h>
#include <X11/Xlib.h>
#include <array>
#include <vector>

#define ENUM_WITH_ALIAS(Identifier, Alias) \
    Identifier, Alias = Identifier

/* actions on NetWmState */
#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

enum {
    NetSupported = 0,
    NetClientList,
    NetClientListStacking,
    NetCloseWindow,
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

typedef enum {
    // see icccm:
    // http://www.x.org/releases/X11R7.7/doc/xorg-docs/icccm/icccm.html#WM_STATE_Property
    WmStateWithdrawnState = 0,
    WmStateNormalState    = 1,
    WmStateIconicState    = 3,
} WmState;

class HSTag;
class Client;
class Root;
class XConnection;

extern Atom g_netatom[NetCOUNT];

extern const std::array<const char*,NetCOUNT> g_netatom_names;

class Ewmh {
public:
    Ewmh(XConnection& xconnection);
    ~Ewmh();

    enum class WM { Name, Protocols, Delete, State, TakeFocus, Last };

    void injectDependencies(Root* root);
    void updateAll();

    void addClient(Window win);
    void removeClient(Window win);
    void updateWmName();

    void updateClientList();
    std::vector<Window> originalClientList() const;
    void updateClientListStacking();
    void updateDesktops();
    void updateDesktopNames();
    void updateActiveWindow(Window win);
    void updateCurrentDesktop();
    void updateWindowState(Client* client);
    void updateFrameExtents(Window win, int left, int right, int top, int bottom);
    bool isWindowStateSet(Window win, Atom hint);
    bool isFullscreenSet(Window win);
    void clearClientProperties(Window win);
    std::string getWindowTitle(Window win);

    int getWindowType(Window win);

    bool isOwnWindow(Window win);
    void clearInputFocus();

    // set the desktop property of a window
    void windowUpdateTag(Window win, HSTag* tag);

    void handleClientMessage(XEvent* event);

    void setWindowOpacity(Window win, double opacity);

    void windowUpdateWmState(Window win, WmState state);

    static Ewmh& get(); // temporary singleton getter

    bool sendEvent(Window window, WM proto, bool checkProtocols);
    void windowClose(Window window);

private:
    bool focusStealingAllowed(long source);
    bool readClientList(Window** buf, unsigned long *count);
    Root* root_ = nullptr;
    XConnection& X_;
    std::vector<Window> original_client_list_; //! client list before hlwm start
    Atom wmatom(WM proto);
    Atom wmatom_[(int)WM::Last];
};

#endif

