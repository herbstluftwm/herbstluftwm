#include "fontdata.h"

#include <X11/Xft/Xft.h>

#include "xconnection.h"

using std::string;

XConnection* FontData::s_xconnection = nullptr;

FontData::~FontData() {
    if (xftFont_ && s_xconnection) {
        XftFontClose(s_xconnection->display(), xftFont_);
    }
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
    // if the font starts with a '-', then treat it as a XLFD and
    // don't pass it to xft
    if (!source.empty() && source[0] != '-') {
        xftFont_ = XftFontOpenName(s_xconnection->display(),
                                   s_xconnection->screen(),
                                   source.c_str());
    }
    if (xftFont_) {
        return;
    }
    // fall back to plain X fonts:
    xFontStruct_ = XLoadQueryFont(s_xconnection->display(), source.c_str());
    if (!xFontStruct_) {
        throw std::invalid_argument(
                string("cannot allocate font \'") + source + "\'");
    }
}
