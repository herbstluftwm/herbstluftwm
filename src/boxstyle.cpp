#include "boxstyle.h"

#include "globals.h"

using std::function;
using std::pair;
using std::string;
using std::vector;

const char BoxStyle::solid[] = "solid";
const char BoxStyle::transparent[] = "transparent";

std::map<std::string, CssValueParser> CssValueParser::propName2Parser_;


template<> CssLen Converter<CssLen>::parse(const string& source)
{
    if (source.size() < 2 || source.substr(source.size() - 2) != "px") {
        throw std::invalid_argument("length must be of the format \"<n>px\", e.g. 4px");
    }
    int pxInt = Converter<int>::parse(source.substr(0, source.size() - 2));
    CssLen len = pxInt;
    return len;
}

template<> std::string Converter<CssLen>::str(CssLen payload)
{
    return Converter<int>::str(payload.inPixels_) + "px";
}

template<> void Converter<CssLen>::complete(Completion&, CssLen const*)
{
}


template<>
Finite<CssDisplay>::ValueList Finite<CssDisplay>::values = ValueListPlain {
    { CssDisplay::none, "none" },
    { CssDisplay::flex, "flex" },
};


/**
 * @brief just an intermediate class to hide
 * templated constructors from the .h file.
 * In a FixedLenParser, only one parser function
 * is set, whereas in CssValueParser, multiple parsers
 * might be set.
 */
class FixedLenParser : public CssValueParser {
public:
    template<typename A>
    FixedLenParser(A BoxStyle::*member)
    {
        parser1_ = [member](const string& arg) {
            A argTyped = Converter<A>::parse(arg);
            return [member,argTyped](BoxStyle& style) -> void{
                style.*member = argTyped;
            };
        };
        getter_ = [member](const BoxStyle& style) -> string {
            return Converter<A>::str(style.*member);
        };
        valuesMatch_ = [member](const BoxStyle& s1, const BoxStyle& s2) -> bool {
            return s1.*member == s2.*member;
        };
    }

    template<typename A1>
    FixedLenParser(std::initializer_list<A1 BoxStyle::*> m1l)
    {
        vector<A1 BoxStyle::*> m1s = m1l;
        parser1_ = [m1s](Str arg1) {
            A1 arg1typed = Converter<A1>::parse(arg1);
            return [m1s,arg1typed](BoxStyle& style) -> void{
                for (auto m1 : m1s) {
                    style.*m1 = arg1typed;
                }
            };
        };
    }

    template<typename A1, typename A2>
    FixedLenParser(std::initializer_list<A1 BoxStyle::*> m1l,
                   std::initializer_list<A2 BoxStyle::*> m2l)
    {
        vector<A1 BoxStyle::*> m1s = m1l;
        vector<A2 BoxStyle::*> m2s = m2l;
        parser2_ = [m1s,m2s](Str arg1, Str arg2) {
            A1 arg1typed = Converter<A1>::parse(arg1);
            A2 arg2typed = Converter<A2>::parse(arg2);
            return [m1s,m2s,arg1typed,arg2typed](BoxStyle& style) -> void{
                for (auto m1 : m1s) {
                    style.*m1 = arg1typed;
                }
                for (auto m2 : m2s) {
                    style.*m2 = arg2typed;
                }
            };
        };
    }

    template<typename A1, typename A2, typename A3, typename A4>
    FixedLenParser(std::initializer_list<A1 BoxStyle::*> m1l,
                   std::initializer_list<A2 BoxStyle::*> m2l,
                   std::initializer_list<A3 BoxStyle::*> m3l,
                   std::initializer_list<A4 BoxStyle::*> m4l)
    {
        vector<A1 BoxStyle::*> m1s = m1l;
        vector<A2 BoxStyle::*> m2s = m2l;
        vector<A3 BoxStyle::*> m3s = m3l;
        vector<A4 BoxStyle::*> m4s = m4l;
        parser4_ = [m1s,m2s,m3s,m4s](Str arg1, Str arg2, Str arg3, Str arg4) {
            A1 arg1typed = Converter<A1>::parse(arg1);
            A2 arg2typed = Converter<A2>::parse(arg2);
            A3 arg3typed = Converter<A3>::parse(arg3);
            A4 arg4typed = Converter<A4>::parse(arg4);
            return [m1s,m2s,m3s,m4s,arg1typed,arg2typed,arg3typed,arg4typed](BoxStyle& style) -> void{
                for (auto m1 : m1s) {
                    style.*m1 = arg1typed;
                }
                for (auto m2 : m2s) {
                    style.*m2 = arg2typed;
                }
                for (auto m3 : m3s) {
                    style.*m3 = arg3typed;
                }
                for (auto m4 : m4s) {
                    style.*m4 = arg4typed;
                }
            };
        };
    }

};

template<typename T>
inline vector<pair<string, FixedLenParser>>
fourSidesParser(string nameAll,
                string nameTop, T BoxStyle::*memberTop,
                string nameRight, T BoxStyle::*memberRight,
                string nameBottom, T BoxStyle::*memberBottom,
                string nameLeft, T BoxStyle::*memberLeft) {

    return {
        {nameTop, memberTop},
        {nameRight, memberRight },
        {nameBottom, memberBottom },
        {nameLeft, memberLeft },
        {nameAll, // setting all 4 sides to the same value
           {{memberTop, memberRight, memberBottom, memberLeft }}},
        {nameAll, // top&bot  left&right
           {{memberTop, memberBottom}, {memberLeft, memberRight }}},
        {nameAll, // setting all separately in one line
           {{memberTop}, {memberRight}, {memberBottom}, {memberLeft}}},
    };
}

static void append_vector(vector<pair<string, FixedLenParser>>& target,
                          const vector<pair<string, FixedLenParser>>& source) {
    target.insert(target.end(), source.cbegin(), source.cend());
}

void CssValueParser::buildParserCache()
{
    vector<pair<string, FixedLenParser>> memberParsers = {
        {"display", &BoxStyle::display },
        {"border-style", &BoxStyle::borderStyle },
        {"min-height", &BoxStyle::minHeight },
        {"min-width", &BoxStyle::minWidth },
        {"outline-style", &BoxStyle::outlineStyle },
        {"background-color", &BoxStyle::backgroundColor },
        {"font", &BoxStyle::font },
        {"color", &BoxStyle::fontColor },
        {"text-align", &BoxStyle::textAlign },
        {"-hlwm-text-depth", &BoxStyle::textDepth },
        {"-hlwm-text-height", &BoxStyle::textHeight },
    };
    append_vector(memberParsers,
                  fourSidesParser(
                      "padding",
                      "padding-top", &BoxStyle::paddingTop,
                      "padding-right", &BoxStyle::paddingRight,
                      "padding-bottom", &BoxStyle::paddingBottom,
                      "padding-left", &BoxStyle::paddingLeft)
                  );
    append_vector(memberParsers,
                  fourSidesParser(
                      "margin",
                      "margin-top", &BoxStyle::marginTop,
                      "margin-right", &BoxStyle::marginRight,
                      "margin-bottom", &BoxStyle::marginBottom,
                      "margin-left", &BoxStyle::marginLeft)
                  );
    append_vector(memberParsers,
                  fourSidesParser(
                      "border-width",
                      "border-top-width", &BoxStyle::borderWidthTop,
                      "border-right-width", &BoxStyle::borderWidthRight,
                      "border-bottom-width", &BoxStyle::borderWidthBottom,
                      "border-left-width", &BoxStyle::borderWidthLeft)
                  );
    append_vector(memberParsers,
                  fourSidesParser(
                      "border-color",
                      "border-top-color", &BoxStyle::borderColorTop,
                      "border-right-color", &BoxStyle::borderColorRight,
                      "border-bottom-color", &BoxStyle::borderColorBottom,
                      "border-left-color", &BoxStyle::borderColorLeft)
                  );
    append_vector(memberParsers,
                  fourSidesParser(
                      "outline-width",
                      "outline-top-width", &BoxStyle::outlineWidthTop,
                      "outline-right-width", &BoxStyle::outlineWidthRight,
                      "outline-bottom-width", &BoxStyle::outlineWidthBottom,
                      "outline-left-width", &BoxStyle::outlineWidthLeft)
                  );
    append_vector(memberParsers,
                  fourSidesParser(
                      "outline-color",
                      "outline-top-color", &BoxStyle::outlineColorTop,
                      "outline-right-color", &BoxStyle::outlineColorRight,
                      "outline-bottom-color", &BoxStyle::outlineColorBottom,
                      "outline-left-color", &BoxStyle::outlineColorLeft)
                  );

    propName2Parser_.clear();
    for (const auto& line : memberParsers) {
        auto it = propName2Parser_.find(line.first);
        if (it == propName2Parser_.end()) {
            propName2Parser_[line.first] = CssValueParser();
            it = propName2Parser_.find(line.first);
            it->second.propertyName_ = line.first;
        }
        if (line.second.getter_) {
            it->second.getter_ = line.second.getter_;
        }
        if (line.second.valuesMatch_) {
            it->second.valuesMatch_ = line.second.valuesMatch_;
        }
        if (line.second.parser1_) {
            it->second.parser1_ = line.second.parser1_;
        }
        if (line.second.parser2_) {
            it->second.parser2_ = line.second.parser2_;
        }
        if (line.second.parser3_) {
            it->second.parser3_ = line.second.parser3_;
        }
        if (line.second.parser4_) {
            it->second.parser4_ = line.second.parser4_;
        }
    }
}

BoxStyle::setter CssValueParser::parse(const std::vector<std::string>& args) const
{
    if (parser1_ && !(parser2_ || parser3_ || parser4_)) {
        // if this property only accepts one argument, then join the args
        std::stringstream buf;
        for (const auto& a : args) {
            buf << a;
        }
        return parser1_(buf.str());
    }
    if (args.size() == 1 && parser1_) {
        return parser1_(args[0]);
    }
    if (args.size() == 2 && parser2_) {
        return parser2_(args[0], args[1]);
    }
    if (args.size() == 3 && parser3_) {
        return parser3_(args[0], args[1], args[2]);
    }
    if (args.size() == 4 && parser4_) {
        return parser4_(args[0], args[1], args[2], args[3]);
    }
    throw std::invalid_argument("property \""
                                + propertyName_
                                + "\" does not accept "
                                + std::to_string(args.size())
                                + " arguments");
}

const CssValueParser& CssValueParser::find(const std::string& propertyName)
{
    if (propName2Parser_.empty()) {
        // built the cache
        buildParserCache();
    }
    // look up the property name
    auto it = propName2Parser_.find(propertyName);
    if (it != propName2Parser_.end()) {
        return it->second;
    } else {
        throw std::invalid_argument("No such property \"" + propertyName + "\"");
    }
}

void CssValueParser::foreachParser(function<void (const CssValueParser&)> loopBody)
{
    if (propName2Parser_.empty()) {
        // built the cache
        buildParserCache();
    }
    for (const auto& it : propName2Parser_) {
        loopBody(it.second);
    }
}
