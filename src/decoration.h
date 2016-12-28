/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __DECORATION_H_
#define __DECORATION_H_

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include "x11-utils.h"
#include <stdbool.h>
#include "x11-types.h"
#include <map>

#include "utils.h"
#include "object.h"
#include "attribute_.h"

class HSClient;


class DecorationScheme : public Object {
public:
    DecorationScheme(std::string name);
    virtual ~DecorationScheme() {};
    Attribute_<unsigned long>     border_width;
    Attribute_<Color>   border_color;
    Attribute_<bool>    tight_decoration; // if set, there is no space between the
                              // decoration and the window content
    Attribute_<Color>   inner_color;
    Attribute_<unsigned long>     inner_width;
    Attribute_<Color>   outer_color;
    Attribute_<unsigned long>     outer_width;
    Attribute_<unsigned long>     padding_top;    // additional window border
    Attribute_<unsigned long>     padding_right;  // additional window border
    Attribute_<unsigned long>     padding_bottom; // additional window border
    Attribute_<unsigned long>     padding_left;   // additional window border
    Attribute_<Color>   background_color; // color behind client contents

    Rectangle inner_rect_to_outline(Rectangle rect) const;
    Rectangle outline_to_inner_rect(Rectangle rect) const;

    // after having called this with some vector 'decs', then if an attribute
    // is changed here, then the attribute with the same name is changed
    // accordingly in each of the elements of 'decs'.
    void makeProxyFor(std::vector<DecorationScheme*> decs);
};

class Decoration {
public:
    Decoration(HSClient* client);
    void createWindow();
    virtual ~Decoration();
    // resize such that the decorated outline of the window fits into rect
    void resize_outline(Rectangle rect, const DecorationScheme& scheme);

    // resize such that the window content fits into rect
    void resize_inner(Rectangle rect, const DecorationScheme& scheme);
    void change_scheme(const DecorationScheme& scheme);

    static HSClient* toClient(Window decoration_window);

    Window decorationWindow() { return decwin; };
    Rectangle last_inner() const { return last_inner_rect; };
    Rectangle last_outer() const { return last_outer_rect; };
    Rectangle inner_to_outer(Rectangle rect);
private:
    void redrawPixmap();
    void updateFrameExtends();
    unsigned int get_client_color(Color color);
private:
    HSClient*        client; // the client to decorate
    Window                  decwin; // the decoration winodw
    const DecorationScheme* last_scheme;
    bool                    last_rect_inner; // whether last_rect is inner size
    Rectangle   last_inner_rect; // only valid if width >= 0
    Rectangle   last_outer_rect; // only valid if width >= 0
    Rectangle   last_actual_rect; // last actual client rect, relative to decoration
    /* X specific things */
    Colormap                colormap;
    unsigned int            depth;
    Pixmap                  pixmap;
    int                     pixmap_height;
    int                     pixmap_width;
    // fill the area behind client with another window that does nothing,
    // especially not repainting or background filling to avoid flicker on
    // unmap
    Window                  bgwin;
private:
    static std::map<Window,HSClient*> decwin2client;
};

class DecTriple : public DecorationScheme {
public:
    DecTriple(std::string name);
    DecorationScheme  normal;
    DecorationScheme  active;
    DecorationScheme  urgent;
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
    DecTriple dec[(int)Type::Count];
    const DecTriple& operator[](Type t) const {
        return dec[(int)t];
    };
    static const Theme& get();
    Theme(std::string name);
};


void decorations_init();
void decorations_destroy();

#endif

