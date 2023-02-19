#ifndef __HERBSTLUFT_EWMH_H_
#define __HERBSTLUFT_EWMH_H_

#include <X11/X.h>
#include <X11/Xlib.h>
#include <array>
#include <string>
#include <vector>

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
    NetWmStateHidden,
    NetWmStateFullscreen,
    NetWmStateDemandsAttention,
    NetWmStateSticky,
    /* window types */
    NetWmWindowTypeDesktop,
    NetWmWindowTypeFIRST = NetWmWindowTypeDesktop,
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
    NetWmWindowTypeNormal,
    NetWmWindowTypeLAST = NetWmWindowTypeNormal,
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

enum class WmState {
    // see icccm:
    // http://www.x.org/releases/X11R7.7/doc/xorg-docs/icccm/icccm.html#WM_STATE_Property
    WSWithdrawnState = 0,
    WSNormalState    = 1,
    WSIconicState    = 3,
};

class HSTag;
class Client;
class Root;
class TagManager;
class XConnection;

/**
 * @brief Implements EWMH and ICCCM communication to other
 * X applications. Although the name only mentions EWMH, this also
 * integrates ICCCM.
 *
 *   EWMH: https://specifications.freedesktop.org/wm-spec/wm-spec-latest.html
 *   ICCCM: https://tronche.com/gui/x/icccm/
 */
class Ewmh {
public:
    Ewmh(XConnection& xconnection);
    ~Ewmh();

    void installWmWindow();

    // ================================================
    // the following functions can be called during bootup:
    // ================================================

    //! initial EWMH state
    class InitialState {
    public:
        size_t numberOfDesktops = 0;
        std::vector<std::string> desktopNames;
        //! client list before hlwm start
        std::vector<Window> original_client_list_;
        void print(FILE* file);
    };

    const InitialState &initialState();
    long windowGetInitialDesktop(Window win);

    /**
     * @brief Check that no other window manager owns the screen
     * selection according to ICCCM 2.8
     * https://tronche.com/gui/x/icccm/sec-2.html#s-2.8
     * @param whether to ask the other WM to stop
     * @return if no other WM is running
     */
    bool acquireScreenSelection(bool replaceExistingWm);
    Atom windowManagerSelection();
    Atom compositingManagerSelection();
    bool detectCompositingManager();
    Window windowManagerWindow() { return windowManagerWindow_; }
    enum class WM { Name, Protocols, Delete, State, ChangeState, TakeFocus, Last };

    void injectDependencies(Root* root);

    // ================================================
    // The following functions may only be called after
    // injectDependencies() was called!
    // ================================================

    void updateAll();

    void addClient(Window win);
    void removeClient(Window win);
    void updateWmName();

    void updateClientList();
    void updateClientListStacking();
    void updateDesktops();
    void updateDesktopNames();
    void updateActiveWindow(Window win);
    void updateCurrentDesktop();
    void updateWindowState(Client* client);
    void updateFloatingState(Client* client);
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

    void handleClientMessage(XClientMessageEvent* me);

    void setWindowOpacity(Window win, double opacity);

    void windowUpdateWmState(Window win, WmState state);

    static Ewmh& get(); // temporary singleton getter

    bool sendEvent(Window window, WM proto, bool checkProtocols);
    void windowClose(Window window);

    XConnection& X() { return X_; }
    Atom netatom(int netatomEnum);
    const char* netatomName(int netatomEnum);

private:
    bool focusStealingAllowed(long source);
    Root* root_ = nullptr;
    TagManager* tags_ = nullptr;
    XConnection& X_;
    InitialState initialState_;
    void readInitialEwmhState();
    Atom wmatom(WM proto);
    Atom wmatom_[(int)WM::Last] = {};
    Atom hlwmFloatingWindow_; //! x11 property set on floated clients
    Atom hlwmTilingWindow_; //! x11 property set on tiled clients

    //! array with Window-IDs in initial mapping order for _NET_CLIENT_LIST
    std::vector<Window> netClientList_;
    //! window that shows that the WM is still alive
    Window      windowManagerWindow_;

    Atom netatom_[NetCOUNT] = {};
    static const std::array<const char*,NetCOUNT> netatomNames_;
};

#endif

