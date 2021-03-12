#include "typesdoc.h"

#include <vector>

#include "attribute_.h"

using std::string;
using std::vector;

class TypeDesc : public Object {
public:
    Attribute_<string> fullname_;
    Attribute_<string> shortname_;

    TypeDesc(Type type)
        : fullname_(this, "fullname", {})
        , shortname_(this, "shortname", {})
    {
        fullname_ = Entity::typestr(type);
        shortname_ = string() + Entity::typechar(type);

        fullname_.setDoc("the full and unique name of this type");
        shortname_.setDoc(
            "A short (one-character long) name of this type "
            "which is used in the output of the 'attr' command"
        );
    }
};

TypesDoc::TypesDoc()
    : bool_(*this, "bool")
    , color_(*this, "color")
    , decimal_(*this, "decimal")
    , font_(*this, "font")
    , int_(*this, "int")
    , names_(*this, "names")
    , rectangle_(*this, "rectangle")
    // , regex_(*this, "regex") // TODO
    , string_(*this, "string")
    , uint_(*this, "uint")
    // , windowid_(*this, "windowid") // TODO
{
    setDoc(
        "This lists the types that are used for attributes and command arguments.");
    bool_.init(Type::BOOL);
    bool_.setDoc(
        "Type representing boolean values, i.e. an 'on' or 'off' state,"
        "with aliases 'true' and 'false'. When writing to a boolean value,"
        "one can also specify 'toggle' in order to alter its value."
    );

    color_.init(Type::COLOR);
    color_.setDoc(
        "Type representing colors.\n"
        "A color can be defined in one of the following formats:\n"
        "1. #RRGGBB where R, G, B are hexidecimal digits (0-9, A-F),\n"
        "   and RR, GG, BB represent the values for red, green, blue.\n"
        "2. #RRGGBBAA represents a color with alpha-value AA.\n"
        "   The alpha value 00 is fully transparent and FF is fully\n"
        "   opaque/intransparent.\n"
        "3. a common color name like 'red', 'blue,' 'orange', etc.\n"
    );

    decimal_.init(Type::DECIMAL);
    decimal_.setDoc("Fixed precision decimal numbers, e.g. 0.34");

    font_.init(Type::FONT);
    font_.setDoc(
         "A font specification (font family with modifiers regarding size, "
         "weight, etc.) in one of the following formats:\n"
         "\n"
         "- Fontconfig description. This supports antialiased fonts,\n"
         "  for example:\n"
         "  * 'Dejavu Sans:pixelsize=12'\n"
         "  * 'Bitstream Vera Sans:size=12:bold'\n"
         "\n"
         "- X logical font description (XLFD), as provided by the\n"
         "  xfontsel tool. No antialiasing is supported here, but this\n"
         "  is usually superior for bitmap fonts. For example:\n"
         "  * '-*-fixed-medium-r-*-*-13-*-*-*-*-*-*-*'\n"
    );

    int_.init(Type::INT);
    int_.setDoc("Type representing signed integers");

    names_.init(Type::NAMES);
    names_.setDoc(
        "A fixed set of names, depending on the context, "
        "e.g. names of layout algorithms or the split type "
        "of a non-leaf frame (which is only 'horizontal' or "
        "'vertical')"
    );

    rectangle_.init(Type::RECTANGLE);
    rectangle_.setDoc(
        "A rectangle on the screen consisting of "
        "a size and the position on the screen. "
        "The format is WxH+X+Y where W is the width, "
        "H is the height, and X and Y are the coordinates "
        "of the top left corner of the rectangle: X is the "
        "number of pixels to the left screen edge and Y is the "
        "number of pixels to the top screen edge. (if X or Y is "
        "negative, then the + turns into -). For example: "
        "800x600+800+0 or 400x200-10+30"
    );

    string_.init(Type::STRING);
    string_.setDoc("Type representing normal text.");

    uint_.init(Type::ULONG);
    uint_.setDoc("Type representing unsigned (i.e. non-negative) integers");
}

TypesDoc::~TypesDoc()
{
    // we need to define the destructor explicitly,
    // because the implicit constructor in the ".h file"
    // fails since TypeDesc is an incomplete type there.
}
