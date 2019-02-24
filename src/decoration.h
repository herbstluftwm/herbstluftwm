#ifndef __DECORATION_H_
#define __DECORATION_H_

#include <X11/X.h>
#include <map>

#include "attribute_.h"
#include "object.h"
#include "x11-types.h"

class Client;
class Settings;


class DecorationScheme : public Object {
public:
    DecorationScheme();
    ~DecorationScheme() override = default;
    Attribute_<unsigned long>     border_width = {"border_width", 1};
    Attribute_<Color>   border_color = {"color", {"black"}};
    Attribute_<bool>    tight_decoration = {"tight_decoration", false}; // if set, there is no space between the
                              // decoration and the window content
    Attribute_<Color>   inner_color = {"inner_color", {"black"}};
    Attribute_<unsigned long>     inner_width = {"inner_width", 0};
    Attribute_<Color>   outer_color = {"outer_color", {"black"}};
    Attribute_<unsigned long>     outer_width = {"outer_width", 0};
    Attribute_<unsigned long>     padding_top = {"padding_top", 0};    // additional window border
    Attribute_<unsigned long>     padding_right = {"padding_right", 0};  // additional window border
    Attribute_<unsigned long>     padding_bottom = {"padding_bottom", 0}; // additional window border
    Attribute_<unsigned long>     padding_left = {"padding_left", 0};   // additional window border
    Attribute_<Color>   background_color = {"background_color", {"black"}}; // color behind client contents

    Rectangle inner_rect_to_outline(Rectangle rect) const;
    Rectangle outline_to_inner_rect(Rectangle rect) const;

    // after having called this with some vector 'decs', then if an attribute
    // is changed here, then the attribute with the same name is changed
    // accordingly in each of the elements of 'decs'.
    void makeProxyFor(std::vector<DecorationScheme*> decs);
};

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

class DecTriple : public DecorationScheme {
public:
    DecTriple();
    DecorationScheme  normal;
    DecorationScheme  active;
    DecorationScheme  urgent;
    // pick the right scheme, depending on whether a window is active/urgent
    const DecorationScheme& operator()(bool if_active, bool if_urgent) const {
        if (if_active) return this->active;
        else if (if_urgent) return this->urgent;
        else return normal;
    }
};

class Theme : public DecTriple {
public:
    enum class Type {
        Fullscreen,
        Tiling,
        Floating,
        Minimal,
        Count,
    };
    const DecTriple& operator[](Type t) const {
        return dec[(int)t];
    };
    Theme();

    // a sub-decoration for each type
    DecTriple dec[(int)Type::Count];
};


#endif

