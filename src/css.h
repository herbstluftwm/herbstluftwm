#pragma once

#include <functional>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "attribute_.h"
#include "boxstyle.h"
#include "commandio.h"
#include "converter.h"
#include "cssname.h"
#include "x11-types.h" // only for Color

class Completion;
class DomTree;

class DomTree {
    /**
     * a very simple iterator to traverse a dom-like tree
     * when testing whether a css selector holds
     */
public:
    virtual ~DomTree() = default;
    virtual const DomTree* parent() const = 0;
    virtual const DomTree* nthChild(size_t idx) const = 0;
    virtual const DomTree* leftSibling() const = 0;
    virtual bool hasClass(const CssName& className) const = 0;
    virtual size_t childCount() const = 0;
};



class CssDeclaration {
public:
    std::string property_;
    std::vector<std::string> values_;
    BoxStyle::setter apply_;

    CssDeclaration() = default;
    CssDeclaration(const BoxStyle::setter& apply)
        : apply_(apply) {
    }
};

class CssSelector {
public:
    class Specifity {
    public:
        // see https://www.w3schools.com/css/css_specificity.asp
        // short idSelectors = 0; // number of id-selectors (not used by hlwm)
        short classSelectors = 0; // number of class or pseudo-class selectors
        // short elementSelectors = 0; // number of element-type selectors (not used by hlwm)
        bool operator<(const Specifity& other) const {
            return classSelectors < other.classSelectors;
        };
    };
    CssSelector() = default;
    CssSelector(std::initializer_list<CssName> content)
        : content_(content) {
    }

    std::vector<CssName> content_;
    bool matches(const DomTree* element) const;
    Specifity specifity() const;
private:
    bool matches(const DomTree* element, size_t prefixLen) const;
};

class CssRuleSet {
public:
    CssRuleSet() = default;
    CssRuleSet(std::initializer_list<CssSelector> selectors,
               std::initializer_list<CssDeclaration> declarations);
    std::vector<CssSelector> selectors_;
    std::vector<CssDeclaration> declarations_;
};

class CssSource {
public:
    std::vector<CssRuleSet> content_;
    void print(std::ostream& out) const;
    std::shared_ptr<BoxStyle> computeStyle(DomTree* element) const;
    void computeStyle(DomTree* element, std::shared_ptr<BoxStyle> target) const;
    void recomputeSortedSelectors();
private:
    class SelectorIndex {
    public:
        size_t indexInContent_ = 0; // the index in CssSource::content_
        size_t indexInSelectors_ = 0; // index in CssRuleSet::selectors_
    };
    using Specifity2Idx = std::pair<CssSelector::Specifity, SelectorIndex>;
    std::vector<Specifity2Idx > sortedSelectors_;

    // dummy required for attribute assignment.
    // we just claim that all CssSource objects
    // are different, so the attribute values are
    // always updated whenever they are assigned.
    // to prevent further damage, we make this operator
    // only available to Attribute_<>.
    friend class Attribute_<CssSource>;
    bool operator!=(const CssSource& other) {
        return this != &other;
    }
};
template<>
inline Type Attribute_<CssSource>::staticType() { return Type::STRING; }

template<> CssSource Converter<CssSource>::parse(const std::string& source);
template<> std::string Converter<CssSource>::str(CssSource payload);
template<> void Converter<CssSource>::complete(Completion& complete, CssSource const* relativeTo);

void debugCssCommand(CallOrComplete invoc);

