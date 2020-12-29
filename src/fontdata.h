#pragma once

#include <X11/Xlib.h>
#include <string>

class XConnection;
struct _XOC;

/**
 * @brief This class has two purposes: 1. Allow different
 * HSFont objects to use the same font resources in X. 2. hide
 * X includes from font.h. So only the decoration renderer
 * needs to include fontdata.h
 */
class FontData {
public:
    FontData() {}
    ~FontData();

    void initFromStr(const std::string& source);

    XFontStruct* xFontStruct_ = nullptr;
    struct _XOC *xFontSet_ = nullptr; // _XOC* = XFontSet

    static XConnection* s_xconnection;
private:
};
