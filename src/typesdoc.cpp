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
    // , font_(*this, "font") // TODO
    , int_(*this, "int")
    , names_(*this, "names")
    // , rectangle_(*this, "rectangle") // TODO
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

    int_.init(Type::INT);
    int_.setDoc("Type representing signed integers");

    names_.init(Type::NAMES);
    names_.setDoc(
        "A fixed set of names, depending on the context, "
        "e.g. names of layout algorithms or the split type "
        "of a non-leaf frame (which is only 'horizontal' or "
        "'vertical')"
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
