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
    , regex_(*this, "regex")
    , string_(*this, "string")
    , uint_(*this, "uint")
    , windowid_(*this, "windowid")
{
    setDoc(
        "This lists the types that are used for attributes and command arguments.");
    bool_.init(Type::BOOL);
    bool_.setDoc(
        "Type representing boolean values, i.e. an 'on' or 'off' state, "
        "with aliases 'true' and 'false'. When writing to a boolean value, "
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
         "  * '-*-fixed-medium-r-*-*-13-*-*-*-*-*-*-*' for a standard \n"
         "    bitmap font available on most systems\n"
    );

    int_.init(Type::INT);
    int_.setDoc(
        "Type representing signed integers.\n"
        "When overwriting an integer, you can increase or decrease its "
        "value relatively by writing \'+=N\' or \'-=N\' where N is an integer. "
        "So for example, writing \'+=3\' to an attribute increases its "
        "value by 3."
    );

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

    regex_.init(Type::REGEX);
    regex_.setDoc(
        "A POSIX extended regular expression. Note that when passing "
        "a regex on the command line, additional quoting can be "
        "necessary. "
        "For explanations and examples, see section 9.4.6 "
        "of the documentation: "
        "https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap09.html#tag_09_04_06"
    );

    string_.init(Type::STRING);
    string_.setDoc("Type representing normal text.");

    uint_.init(Type::ULONG);
    uint_.setDoc(
        "Type representing unsigned (i.e. non-negative) integers.\n"
        "When overwriting an integer, you can increase or decrease its "
        "value relatively by writing \'+=N\' or \'-=N\' where N is an integer."
    );

    windowid_.init(Type::WINDOW);
    windowid_.setDoc(
        "The window id is the number of a window. This can be a managed window "
        "(i.e. client) or an unmanaged window (e.g. a panel, a menu, "
        "or a desktop window).\n"
        "The default format is 0xHEX where HEX is a hexadecimal number "
        "(digits 0-9 and a-f) but it can also be specified in the decimal "
        "system (base 10), or as an octal number (with prefix 0 and base 8).\n"
        "When a window id is printed, it is always printed in the 0xHEX format "
        "and without any leading zeroes."
    );
}

TypesDoc::~TypesDoc()
{
    // we need to define the destructor explicitly,
    // because the implicit constructor in the ".h file"
    // fails since TypeDesc is an incomplete type there.
}
