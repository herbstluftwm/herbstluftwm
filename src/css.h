#pragma once

#include <functional>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "attribute_.h"
#include "converter.h"
#include "commandio.h"
#include "x11-types.h" // only for Color

class Completion;
class DomTree;

class ComputedStyle {
public:
    short borderWidthTop = 2;
    short borderWidthRight = 2;
    short borderWidthBottom = 2;
    short borderWidthLeft = 2;
    short paddingTop = 2;
    short paddingRight = 2;
    short paddingBottom = 2;
    short paddingLeft = 2;
    Color backgroundColor = {};
    Color borderColor = {};
    static const ComputedStyle empty;
    using setter = std::function<void(ComputedStyle&)>;
};

class CssLen {
public:
    short inPixels_;
};

template<> CssLen Converter<CssLen>::parse(const std::string& source);
template<> std::string Converter<CssLen>::str(CssLen payload);
template<> void Converter<CssLen>::complete(Completion& complete, CssLen const* relativeTo);

class CssName {
public:
    enum class Builtin {
        /* CSS Combinators */
        child,
        descendant,
        adjacent_sibling,
        LAST_COMBINATOR = adjacent_sibling,
        /* built in names */
        tabs,
        client_content,
        focus,
        window,
        LAST = window,
    };
    Builtin special_ = Builtin::LAST;
    std::string custom_;
    static const std::vector<std::pair<Builtin, std::string>> specialNames;
};


template<> CssName Converter<CssName>::parse(const std::string& source);
template<> std::string Converter<CssName>::str(CssName payload);
template<> void Converter<CssName>::complete(Completion& complete, CssName const* relativeTo);


class CssNameSet {
public:
    unsigned long long int names_ = 0; // at least 64 bits
};


class CssDeclaration {
public:
    std::string property_;
    std::vector<std::string> values_;
    ComputedStyle::setter apply_;
};

class CssSelector {
public:
    std::vector<CssName> content_;
};

class CssRuleSet {
public:
    std::vector<CssSelector> selectors_;
    std::vector<CssDeclaration> declarations_;
};

class DomTree {
    /**
     * a very simple iterator to traverse a dom-like tree
     * when testing whether a css selector holds
     */
public:
    virtual ~DomTree() = default;
    virtual DomTree* parent() = 0;
    virtual DomTree* nthChild(size_t idx) = 0;
    virtual DomTree* leftSibling() = 0;
    virtual bool hasClass(const std::string& className) = 0;
    virtual size_t childCount() = 0;
};


class CssSource {
public:
    std::vector<CssRuleSet> content_;
    void print(std::ostream& out) const;
private:
    // dummy required for attribute assignment.
    // we just claim that all CssSource objects
    // are different, so the attribute values are
    // always updated whenever they are assigned.
    // to prevent further damage, we make this operator
    // only available to Attribute_<>.
    friend class Attribute_<CssSource>;
    bool operator!=(const CssSource&) {
        return true;
    }
};
template<>
inline Type Attribute_<CssSource>::staticType() { return Type::STRING; }

template<> CssSource Converter<CssSource>::parse(const std::string& source);
template<> std::string Converter<CssSource>::str(CssSource payload);
template<> void Converter<CssSource>::complete(Completion& complete, CssSource const* relativeTo);

void debugCssCommand(CallOrComplete invoc);

