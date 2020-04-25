#include "fontdata.h"

#include "xconnection.h"

using std::string;

XConnection* FontData::s_xconnection = nullptr;

FontData::~FontData() {
    if (xFontStruct_ && s_xconnection) {
        XFreeFont(s_xconnection->display(), xFontStruct_);
    }
}

//! try to parse a font description or throw an exception
void FontData::initFromStr(const string& source)
{
    if (!s_xconnection) {
        throw std::invalid_argument("X connection not established yet!");
    }
    xFontStruct_ = XLoadQueryFont(s_xconnection->display(), source.c_str());
    if (!xFontStruct_) {
        throw std::invalid_argument(
                string("cannot allocate font \'") + source + "\'");
    }
}
