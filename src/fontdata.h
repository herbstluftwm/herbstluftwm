#pragma once

#include <X11/Xlib.h>
#include <string>

struct _XftFont;
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
    int textwidth(const std::string& text, size_t len) const;
    int ascent = 0; //! pixels above baseline
    int descent = 0; //! pixels below baseline

    struct _XftFont* xftFont_ = nullptr;
    XFontStruct* xFontStruct_ = nullptr;
    struct _XOC *xFontSet_ = nullptr; // _XOC* = XFontSet

    static XConnection* s_xconnection;
private:
    // loaders for different fonts. They load the font into the
    // pointers above, and return a boolean indicating success.
    // on error, they may still throw an exception
    bool loadXftFont(XConnection& xcon, const std::string& source);
    bool loadXFontSet(XConnection& xcon, const std::string& source);
    bool loadXFontStruct(XConnection& xcon, const std::string& source);
};
