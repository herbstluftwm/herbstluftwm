#pragma once

#include <functional>
#include <ostream>
#include <string>
#include <vector>

#include "commandio.h"

class DomTree;

class CssBox {
public:
    short borderWidthTop;
    short borderWidthRight;
    short borderWidthBottom;
    short borderWidthLeft;
};


class CssDeclaration {
public:
    std::string property_;
    std::vector<std::string> values_;
    // std::function<void(CssBox& target)> apply_;
};

class CssSelector {
public:
    std::vector<std::string> content_;
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


class CssFile {
public:
    std::vector<CssRuleSet> content_;
    void print(std::ostream& out) const;
};

void debugCssCommand(CallOrComplete invoc);

