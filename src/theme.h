#pragma once

#include <string>
#include <vector>

#include "attribute_.h"
#include "object.h"

class DecorationScheme : public Object {
public:
    DecorationScheme();
    ~DecorationScheme() override = default;
    DynAttribute_<std::string> reset;
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
private:
    std::string resetSetterHelper(std::string dummy);
    std::string resetGetterHelper();
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


