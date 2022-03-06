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
    void print(std::ostream& out) const {
        out << "    " << property_ << ":";
        for (const auto& v : values_) {
            out << " " << v;
        }
        out << ";\n";
    }
};

class CssSelector {
public:
    std::vector<std::string> content_;
    void print(std::ostream& out) const {
        for (const auto& v : content_) {
            out << v;
        }
    }
};

class CssRuleSet {
public:
    std::vector<CssSelector> selectors_;
    std::vector<CssDeclaration> declarations_;
    void print(std::ostream& out) const {
        bool first = true;
        for (const auto& sel : selectors_) {
            if (!first) {
                out << " ,\n";
            }
            sel.print(out);
            first = false;
        }
        out << " {\n";
        for (const auto& decl : declarations_) {
            decl.print(out);
        }
        out << "}\n";
    }
};


class CssFile {
public:
    std::vector<CssRuleSet> content_;
    void print(std::ostream& out) const {
        bool first = true;
        for (const auto& block : content_) {
            if (!first) {
                out << "\n";
            }
            block.print(out);
            first = false;
        }
    }
};

void debugCssCommand(CallOrComplete invoc);

