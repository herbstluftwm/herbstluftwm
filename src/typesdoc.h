#pragma once

#include "child.h"
#include "object.h"

class TypeDesc;

/**
 * @brief expose meta-information on the types via the object tree.
 *
 * Its main purpose is that the auto-generated documentation describes
 * the types and their syntax. Hence, the documentation on the types
 * is in the man page and in the interactive help, thus one can run
 * for example `herbstclient help types.color` in order to obtain
 * information on the syntax of colors.
 */
class TypesDoc : public Object
{
public:
    TypesDoc();
    ~TypesDoc();
private:
    Child_<TypeDesc> bool_;
    Child_<TypeDesc> color_;
    Child_<TypeDesc> decimal_;
    // Child_<TypeDesc> font_; // TODO
    Child_<TypeDesc> int_;
    // Child_<TypeDesc> names_; // TODO
    // Child_<TypeDesc> rectangle_; // TODO
    // Child_<TypeDesc> regex_; // TODO
    Child_<TypeDesc> string_;
    Child_<TypeDesc> uint_;
    // Child_<TypeDesc> windowid_; // TODO
};
