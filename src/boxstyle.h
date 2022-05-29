#ifndef BOXSTYLE_H
#define BOXSTYLE_H

#include <functional>
#include <string>
#include <vector>

#include "x11-types.h"

class CssLen {
public:
    CssLen(int px) : inPixels_(static_cast<short>(px)) {}
    short inPixels_;
    operator int() const {
        return static_cast<int>(inPixels_);
    }
};

template<> CssLen Converter<CssLen>::parse(const std::string& source);
template<> std::string Converter<CssLen>::str(CssLen payload);
template<> void Converter<CssLen>::complete(Completion& complete, CssLen const* relativeTo);


/**
 * @brief The BoxStyle specifies how to draw a single
 * css box (in our case: widget).
 */
class BoxStyle {
public:
    CssLen borderWidthTop = 2;
    CssLen borderWidthRight = 2;
    CssLen borderWidthBottom = 2;
    CssLen borderWidthLeft = 2;
    CssLen paddingTop = 2;
    CssLen paddingRight = 2;
    CssLen paddingBottom = 2;
    CssLen paddingLeft = 2;
    Color backgroundColor = {};
    Color borderColor = {};
    static const BoxStyle empty;
    using setter = std::function<void(BoxStyle&)>;
};

class CssValueParser {
public:
    CssValueParser() {}
    //! try to parse the given args or throw an std::invalid_argument exception
    BoxStyle::setter parse(const std::vector<std::string>& args) const;
    //! find a parser for a propertyName or throw an std::invalid_argument exception
    static const CssValueParser& find(const std::string& propertyName);

protected:
    std::string propertyName_;
    using Str = const std::string&;
    std::function<BoxStyle::setter(Str)> parser1_ = {};
    std::function<BoxStyle::setter(Str,Str)> parser2_ = {};
    std::function<BoxStyle::setter(Str,Str,Str)> parser3_ = {};
    std::function<BoxStyle::setter(Str,Str,Str,Str)> parser4_ = {};

private:
    static std::map<std::string, CssValueParser> propName2Parser_;
    static void buildParserCache();
};


#endif // BOXSTYLE_H
