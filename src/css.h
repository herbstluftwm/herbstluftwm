#pragma once

#include <functional>
#include <ostream>
#include <string>
#include <vector>

#include "commandio.h"


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


class CssFile {
public:
    std::vector<CssRuleSet> content_;
    void print(std::ostream& out) const;
};

void debugCssCommand(CallOrComplete invoc);

