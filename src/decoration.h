#ifndef __DECORATION_H_
#define __DECORATION_H_

#include <X11/X.h>
#include <map>

#include "x11-types.h"

class Client;
class Settings;
class DecorationScheme;

class Decoration {
public:
    Decoration(Client* client_, Settings& settings_);
    void createWindow();
    virtual ~Decoration();
    // resize such that the decorated outline of the window fits into rect
    void resize_outline(Rectangle rect, const DecorationScheme& scheme);

    // resize such that the window content fits into rect
    void resize_inner(Rectangle rect, const DecorationScheme& scheme);
    void change_scheme(const DecorationScheme& scheme);

    static Client* toClient(Window decoration_window);

    Window decorationWindow() { return decwin; }
    Rectangle last_inner() const { return last_inner_rect; }
    Rectangle last_outer() const { return last_outer_rect; }
    Rectangle inner_to_outer(Rectangle rect);

private:
    void redrawPixmap();
    void updateFrameExtends();
    unsigned int get_client_color(Color color);

    Window                  decwin = 0; // the decoration window
    const DecorationScheme* last_scheme = {};
    bool                    last_rect_inner; // whether last_rect is inner size
    Rectangle   last_inner_rect; // only valid if width >= 0
    Rectangle   last_outer_rect; // only valid if width >= 0
    Rectangle   last_actual_rect; // last actual client rect, relative to decoration
    /* X specific things */
    Colormap                colormap;
    unsigned int            depth;
    Pixmap                  pixmap = 0;
    int                     pixmap_height = 0;
    int                     pixmap_width = 0;
    // fill the area behind client with another window that does nothing,
    // especially not repainting or background filling to avoid flicker on
    // unmap
    Window                  bgwin;
private:
    Client* client_; // the client to decorate
    Settings& settings_;
    static std::map<Window,Client*> decwin2client;
};

#endif

