#ifndef __DECORATION_H_
#define __DECORATION_H_

#include <X11/X.h>
#include <map>

#include "rectangle.h"
#include "x11-types.h"

class Client;
class FontData;
class Settings;
class DecorationScheme;
class XConnection;

class Decoration {
public:
    Decoration(Client* client_, Settings& settings_);
    void createWindow();
    virtual ~Decoration();
    // resize such that the decorated outline of the window fits into rect
    void resize_outline(Rectangle outline, const DecorationScheme& scheme);

    // resize such that the window content fits into rect
    void resize_inner(Rectangle inner, const DecorationScheme& scheme);
    void change_scheme(const DecorationScheme& scheme);
    void redraw();

    static Client* toClient(Window decoration_window);

    Window decorationWindow() { return decwin; }
    Rectangle last_inner() const { return last_inner_rect; }
    Rectangle last_outer() const { return last_outer_rect; }
    Rectangle inner_to_outer(Rectangle rect);

    bool positionTriggersResize(Point2D p);

private:
    static Visual* check_32bit_client(Client* c);
    static XConnection& xconnection();
    void redrawPixmap();
    void updateFrameExtends();
    unsigned long get_client_color(Color color);

    void drawText(Pixmap& pix, GC& gc, const FontData& fontData,
                  const Color& color, Point2D position, const std::string& text);

    Window                  decwin = 0; // the decoration window
    const DecorationScheme* last_scheme = {};
    bool                    last_rect_inner = false; // whether last_rect is inner size
    Rectangle   last_inner_rect = {0, 0, 0, 0}; // only valid if width >= 0
    Rectangle   last_outer_rect = {0, 0, 0, 0}; // only valid if width >= 0
    Rectangle   last_actual_rect = {0, 0, 0, 0}; // last actual client rect, relative to decoration
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
private:
    Client* client_; // the client to decorate
    Settings& settings_;
    static std::map<Window,Client*> decwin2client;
};

#endif

