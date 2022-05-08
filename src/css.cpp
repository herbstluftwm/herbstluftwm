#include "css.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <tuple>

#include "argparse.h"
#include "globals.h"
#include "parserutils.h"

using std::endl;
using std::function;
using std::make_pair;
using std::pair;
using std::string;
using std::stringstream;
using std::vector;

Parser<CssFile> cssFileParser() {
    auto nextToken = [](SourceStream& source) -> string {
        return source.nextToken("/*{}>~,;+:");
    };
    Parser<CssDeclaration> parseDecl = {
        [nextToken] (SourceStream& source) {
            CssDeclaration decl;
            decl.property_ = nextToken(source);
            source.skipWhitespace();
            source.consumeOrException(":");
            source.skipWhitespace();
            while (!source.startswith(";") && !source.startswith("}")) {
                decl.values_.push_back(nextToken(source));
                source.skipWhitespace();
            }
            source.skipWhitespace();
            return decl;
    }};
    Parser<CssSelector> parseSelector = {
        [nextToken] (SourceStream& source) {
            source.skipWhitespace();
            CssSelector selector;
            while (!source.startswith("{") && !source.startswith("}") && !source.startswith(",")) {
                string tok = nextToken(source);
                selector.content_.push_back(tok);
                if (source.skipWhitespace() > 0) {
                    selector.content_.push_back(" ");
                }
            }
            // remove whitespace tokens at end:
            while (!selector.content_.empty() && *(selector.content_.rbegin()) == " ") {
                selector.content_.pop_back();
            }
            if (selector.content_.empty()) {
                source.raise("a selector must not be empty");
            }
            return selector;
        }
    };
    Parser<CssRuleSet> parseRuleSet = {
        [parseSelector, parseDecl] (SourceStream& source) {
            CssRuleSet ruleSet;
            ruleSet.selectors_ = parseSelector.notApplicableFor("{").sepEndBy(',')(source);
            if (ruleSet.selectors_.empty()) {
                source.raise("need at least one selector");
            }
            source.skipWhitespace();
            source.consumeOrException("{");
            source.skipWhitespace();
            ruleSet.declarations_ = parseDecl.notApplicableFor("}").sepEndBy(';')(source);
            source.skipWhitespace();
            source.consumeOrException("}");
            source.skipWhitespace();
            return ruleSet;
    }};
    return {
        [parseRuleSet] (SourceStream& source) {
            CssFile file;
            source.skipWhitespace();
            file.content_ = parseRuleSet.many()(source);
            source.skipWhitespace();
            if (!source.isEOF()) {
                source.expectedButGot("EOF");
            }
            return file;
        }
    };
}

class DummyTree {
public:
    vector<string> classes_ = {};
    vector<DummyTree> children_ = {};
    static DummyTree parse(SourceStream& source) {
        source.consumeOrException("(");
        DummyTree tree;
        source.skipWhitespace();
        while (!source.isEOF() && !source.startswith(")")) {
            if (source.startswith("(")) {
                tree.children_.push_back(parse(source));
            } else {
                tree.classes_.push_back(source.nextToken("() "));
            }
            source.skipWhitespace();
        }
        source.consumeOrException(")");
        return tree;
    }
    void print(std::ostream& output, int indent = 0) const {
        for (int i = 0; i < indent ; i++) {
            output << "  ";
        }
        output << "(";
        bool first = true;
        for (const auto& cls : classes_) {
            if (!first) {
                output << " ";
            }
            output << cls;
            first = false;
        }
        for (const auto& child : children_) {
            output << "\n";
            child.print(output, indent + 1);
        }
        output << ")";
    }
};

template<>
DummyTree Converter<DummyTree>::parse(const string& source) {
    try {
        SourceStream stream = SourceStream::fromString(source);
        stream.skipWhitespace();
        DummyTree tree = DummyTree::parse(stream);
        stream.skipWhitespace();
        if (!stream.isEOF()) {
            stream.expectedButGot("EOF");
        }
        return tree;
    } catch (const SourceStream::Error& error) {
        throw std::runtime_error(error.str());
    }
}

template<>
string Converter<DummyTree>::str(DummyTree payload) {
    stringstream output;

    payload.print(output);
    return output.str();
}

template<>
void Converter<DummyTree>::complete(Completion& complete, const DummyTree*) {
}



void CssFile::print(std::ostream& out) const
{
    int idx = 0;
    for (const auto& ruleset : content_) {
        if (idx++ > 0) {
            out << "\n";
        }
        // print selector
        int selectorIdx = 0;
        for (const auto& selector : ruleset.selectors_) {
            if (selectorIdx++ > 0) {
                out << " ,\n";
            }
            for (const auto& s: selector.content_) {
                out << s;
            }
        }
        // print block
        out << " {\n";
        for (const auto& decl: ruleset.declarations_) {
            out << "    " << decl.property_ << ":";
            for (const auto& v : decl.values_) {
                out << " " << v;
            }
            out << ";\n";
        }
        out << "}\n";
    }
}

void debugCssCommand(CallOrComplete invoc)
{
    string cssSource;
    bool print = false, printTree = false;
    DummyTree tree;
    ArgParse ap;
    ap.mandatory(cssSource);
    ap.flags({
        {"--print-css", &print },
        {"--tree=", tree },
        {"--print-tree", &printTree },
    });
    ap.command(invoc,
        [&] (Output output) {
            Parser<CssFile> parser = cssFileParser();
            CssFile file;
            try {
                auto stream = SourceStream::fromString(cssSource);
                file = parser(stream);
            } catch (const SourceStream::Error& error) {
                output.error() << error.str();
                return 1;
            }
            if (print) {
                file.print(output.output());
            }
            if (printTree) {
                output << Converter<DummyTree>::str(tree) << endl;
            }
            return 0;
    });
}

