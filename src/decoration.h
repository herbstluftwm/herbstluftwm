#ifndef __DECORATION_H_
#define __DECORATION_H_

#include <X11/X.h>
#include <map>

#include "optional.h"
#include "rectangle.h"
#include "widget.h"
#include "x11-types.h"

class Client;
class Settings;
class DecorationScheme;
class Theme;
class XConnection;

class ResizeAction {
public:
    bool left = false;
    bool right = false;
    bool top = false;
    bool bottom = false;
    operator bool() const {
        return left || right || top || bottom;
    }
    std::experimental::optional<unsigned int> toCursorShape() const;
    ResizeAction operator*(const ResizeAction& other) {
        ResizeAction act;
        act.left = left && other.left;
        act.right = right && other.right;
        act.top = top && other.top;
        act.bottom = bottom && other.bottom;
        return act;
    }
};


class TabWidget : public Widget {
public:
    TabWidget();
    Client* tabClient = nullptr;
    virtual std::string textContent() const override;
};

/**
 * @brief the parameters that affect the look of decorations
 */
class DecorationParameters {
public:
    bool focused_ = false;
    bool fullscreen_ = false;
    bool pseudotiled_ = false;
    bool floating_ = false;
    bool minimal_ = false;
    bool urgent_ = false;
    std::vector<Client*> tabs_;
    std::vector<bool> urgentTabs_;

    bool operator==(const DecorationParameters& other) const {
        return focused_ == other.focused_
                && fullscreen_ == other.fullscreen_
                && pseudotiled_ == other.pseudotiled_
                && floating_ == other.floating_
                && minimal_ == other.minimal_
                && tabs_ == other.tabs_
                && urgentTabs_ == other.urgentTabs_;
    }
    void updateTabUrgencyFlags();
    void removeClient(Client* client);;
};


class Decoration {
public:
    class ClickArea {
    public:
        Rectangle area_ = {}; //! where to click
        Client* tabClient_ = {}; //! the client that will get activated
    };

    Decoration(Client* client, Settings& settings, Theme& theme);
    void createWindow();
    virtual ~Decoration();
    void setParameters(const DecorationParameters& params, bool force = false);
    void computeWidgetGeometries(Rectangle innerGeometry);
    // resize such that the decorated outline of the window fits into rect
    void resize_outline(Rectangle outline);
    void applyWidgetGeometries();

    // resize such that the window content fits into rect
    void resize_inner(Rectangle inner);
    void redraw();

    static Client* toClient(Window decoration_window);

    Window decorationWindow() { return decwin; }
    Rectangle last_inner() const;
    inline Rectangle last_outer() const { return widMain.geometryCached(); }
    Rectangle inner_to_outer(Rectangle rect);

    void updateResizeAreaCursors();

    std::experimental::optional<ClickArea> positionHasButton(Point2D p);
    ResizeAction positionTriggersResize(Point2D p);
    ResizeAction resizeFromRoughCursorPosition(Point2D cursor);
    void removeFromTabBar(Client* otherClientTab);

private:
    Widget widMain;
    Widget widPanel;
    Widget widTabBar;
    Widget widClient;
    DecorationParameters lastParams;
    int borderWidth() const;
    std::vector<TabWidget*> widTabs;
    static Visual* check_32bit_client(Client* c);
    static XConnection& xconnection();
    void redrawPixmap();
    void updateFrameExtends();

    Window                  decwin = 0; // the decoration window
    Rectangle   last_outer_rect = {0, 0, 0, 0}; // only valid if width >= 0
    /* X specific things */
    Visual*                 visual = nullptr;
    Colormap                colormap = 0;
    unsigned int            depth = 0;
    Pixmap                  pixmap = 0;
    int                     pixmap_height = 0;
    int                     pixmap_width = 0;
    // fill the area behind client with another window that does nothing,
    // especially not repainting or background filling to avoid flicker on
    // unmap
    Window                  bgwin = 0;
    /** 12 = 4 sides with 3 sections each. */
    static constexpr size_t resizeAreaSize = 4 * 3;
    Window                  resizeArea[resizeAreaSize] = {};
    static ResizeAction resizeAreaInfo(size_t idx);
    Rectangle resizeAreaGeometry(size_t idx, int borderWidth, int width, int height);
private:
    Client* client_; // the client to decorate
    Settings& settings_;
    Theme& theme_;
    static std::map<Window,Client*> decwin2client;
};

#endif

