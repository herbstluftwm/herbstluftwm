#include "fontdata.h"

#include <sstream>

#include "globals.h"
#include "xconnection.h"

using std::string;
using std::stringstream;

XConnection* FontData::s_xconnection = nullptr;

FontData::~FontData() {
    if (xFontStruct_ && s_xconnection) {
        XFreeFont(s_xconnection->display(), xFontStruct_);
    }
    if (xFontSet_ && s_xconnection) {
        XFreeFontSet(s_xconnection->display(), xFontSet_);
    }
}

//! try to parse a font description or throw an exception
void FontData::initFromStr(const string& source)
{
    if (!s_xconnection) {
        throw std::invalid_argument("X connection not established yet!");
    }

    // plain X fonts with unicode support
    char** missingCharSetList = nullptr;
    int missingCharSetCount = 0;
    char* defString = nullptr;
    xFontSet_ = XCreateFontSet(s_xconnection->display(), source.c_str(),
                               &missingCharSetList, &missingCharSetCount,
                               &defString);
    stringstream msg;
    if (missingCharSetCount > 0) {
        msg << "The following charsets are unknown: ";
        for (int i = 0; i < missingCharSetCount; i++) {
            if (i != 0) {
                msg << ", ";
            }
            msg << missingCharSetList[i];
        }
        XFreeStringList(missingCharSetList);
    }
    if (xFontSet_) {
        if (missingCharSetCount > 0) {
            HSWarning("When loading font \"%s\": %s\n", source.c_str(), msg.str().c_str());
        }
        return;
    } else {
        if (missingCharSetCount > 0) {
            throw std::invalid_argument(msg.str());
        }
    }

    xFontStruct_ = XLoadQueryFont(s_xconnection->display(), source.c_str());
    if (!xFontStruct_) {
        throw std::invalid_argument(
                string("cannot allocate font \'") + source + "\'");
    }
}
