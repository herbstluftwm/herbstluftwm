#ifndef BOXSTYLE_H
#define BOXSTYLE_H

#include <functional>
#include <string>
#include <vector>

#include "either.h"
#include "font.h"
#include "unit.h"
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


enum class CssDisplay {
    none,
    flex,
};

template <>
struct is_finite<CssDisplay> : std::true_type {};
template<> Finite<CssDisplay>::ValueList Finite<CssDisplay>::values;



/**
 * @brief The BoxStyle specifies how to draw a single
 * css box (in our case: widget).
 */
class BoxStyle {
public:
    static const char auto_[];
    static const char solid[];
    static const char transparent[];

    Either<Unit<transparent>,Color> backgroundColor = Unit<transparent>();

    CssDisplay display = CssDisplay::flex;

    CssLen minWidth = 0;
    CssLen minHeight = 0;

    CssLen paddingTop = 0;
    CssLen paddingRight = 0;
    CssLen paddingBottom = 0;
    CssLen paddingLeft = 0;
    CssLen marginTop = 0;
    CssLen marginRight = 0;
    CssLen marginBottom = 0;
    CssLen marginLeft = 0;

    CssLen borderWidthTop = 0;
    CssLen borderWidthRight = 0;
    CssLen borderWidthBottom = 0;
    CssLen borderWidthLeft = 0;

    Unit<solid> borderStyle; // only solid border style supported currently

    Color borderColorTop = {};
    Color borderColorRight = {};
    Color borderColorBottom = {};
    Color borderColorLeft = {};

    CssLen outlineWidthTop = 0;
    CssLen outlineWidthRight = 0;
    CssLen outlineWidthBottom = 0;
    CssLen outlineWidthLeft = 0;

    Unit<solid> outlineStyle; // only solid border style supported currently

    Color outlineColorTop = {};
    Color outlineColorRight = {};
    Color outlineColorBottom = {};
    Color outlineColorLeft = {};

    Color fontColor = {};
    TextAlign textAlign = TextAlign::left;
    Either<Unit<auto_>,CssLen> textDepth = Unit<auto_>();
    Either<Unit<auto_>,CssLen> textHeight = Unit<auto_>();
    HSFont font = HSFont::fromStr("");

    std::map<std::string, std::string> changedProperties() const;
    static const BoxStyle empty() {
        return {};
    }
    using setter = std::function<void(BoxStyle&)>;
    void inheritFromParent(std::shared_ptr<const BoxStyle> parentStyle);
};

class CssValueParser {
public:
    CssValueParser() {}
    //! try to parse the given args or throw an std::invalid_argument exception
    BoxStyle::setter parse(const std::vector<std::string>& args) const;
    //! find a parser for a propertyName or throw an std::invalid_argument exception
    static const CssValueParser& find(const std::string& propertyName);
    static void foreachParser(std::function<void(const CssValueParser&)> loopBody);

    // if the parser refers to only one member, then
    // the following functions are set:
    //! return the member as a string
    std::function<std::string(const BoxStyle&)> getter_ = {};
    //! tell whether the two box styles match on this member
    std::function<bool(const BoxStyle&, const BoxStyle&)> valuesMatch_ = {};
    std::string name() const { return propertyName_; }
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
