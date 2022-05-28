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

const ComputedStyle ComputedStyle::empty;

const vector<pair<CssName::Builtin, std::string>> CssName::specialNames =
{
    { CssName::Builtin::child, ">" },
    { CssName::Builtin::descendant, " " },
    { CssName::Builtin::adjacent_sibling, "+" },
    { CssName::Builtin::tabs, "tabbar" },
    { CssName::Builtin::client_content, "client_content" },
    { CssName::Builtin::window, "window" },
    { CssName::Builtin::focus, "focus" },
};

class CssValueParser {
public:
    CssValueParser() {}
    // template<typename T>
    // CssValueParser(T ComputedStyle::*styleMember) {
    //     parameterCount_ = 1;
    //     parser_ = [styleMember](const vector<string>& args) {
    //         T arg1 = Converter<T>::parse(args.empty() ? "" : args[0]);
    //         return [styleMember,arg1](ComputedStyle& style) -> void {
    //             style.*styleMember = arg1;
    //         };
    //     };
    // }
    size_t parameterCount_ = 0;
    function<ComputedStyle::setter(const vector<string>&)> parser_ = {};
};

// CssValueParser tmp = myfun;
template<typename A>
CssValueParser P(function<void(ComputedStyle&,A)> typedSetter)
{
    CssValueParser cvp;
    cvp.parameterCount_ = 1;
    cvp.parser_ = [typedSetter](const vector<string>& args) {
        A arg1 = Converter<A>::parse(args.empty() ? "" : args[0]);
        return [typedSetter,arg1](ComputedStyle& style) -> void{
            typedSetter(style, arg1);
        };
    };
    return cvp;
}

template<typename A, typename B>
CssValueParser P(function<void(ComputedStyle&,A,B)> typedSetter)
{
    CssValueParser cvp;
    cvp.parameterCount_ = 2;
    cvp.parser_ = [typedSetter](const vector<string>& args) {
        A arg1 = Converter<A>::parse((args.size() < 2) ? "" : args[0]);
        B arg2 = Converter<B>::parse((args.size() < 2) ? "" : args[1]);
        return [typedSetter,arg1,arg2](ComputedStyle& style) -> void{
            typedSetter(style, arg1, arg2);
        };
    };
    return cvp;
}

template<typename A, typename B, typename C, typename D>
CssValueParser P(function<void(ComputedStyle&,A,B,C,D)> typedSetter)
{
    CssValueParser cvp;
    cvp.parameterCount_ = 4;
    cvp.parser_ = [typedSetter](const vector<string>& args) {
        A arg1 = Converter<A>::parse((args.size() < 4) ? "" : args[0]);
        B arg2 = Converter<B>::parse((args.size() < 4) ? "" : args[1]);
        C arg3 = Converter<C>::parse((args.size() < 4) ? "" : args[2]);
        D arg4 = Converter<D>::parse((args.size() < 4) ? "" : args[3]);
        return [typedSetter,arg1,arg2,arg3,arg4](ComputedStyle& style) -> void{
            typedSetter(style, arg1, arg2, arg3, arg4);
        };
    };
    return cvp;
}

const vector<pair<string, CssValueParser>> cssValueParsers = {
    {"border-width", P<CssLen>([](ComputedStyle& style, CssLen len) {
         style.borderWidthLeft = len.inPixels_;
         style.borderWidthRight = len.inPixels_;
         style.borderWidthTop = len.inPixels_;
         style.borderWidthBottom = len.inPixels_;
     })},
    {"border-width", P<CssLen,CssLen>([](ComputedStyle& style, CssLen topBot, CssLen leftRight) {
         style.borderWidthLeft = leftRight.inPixels_;
         style.borderWidthRight = leftRight.inPixels_;
         style.borderWidthTop = topBot.inPixels_;
         style.borderWidthBottom = topBot.inPixels_;
     })},
    {"border-width", P<CssLen,CssLen,CssLen,CssLen>([](ComputedStyle& style, CssLen top, CssLen right, CssLen bot, CssLen left) {
         style.borderWidthLeft = left.inPixels_;
         style.borderWidthRight = right.inPixels_;
         style.borderWidthTop = top.inPixels_;
         style.borderWidthBottom = bot.inPixels_;
     })},
    {"border-top-width", P<CssLen>([](ComputedStyle& style, CssLen len) {
         style.borderWidthTop = len.inPixels_;
     })},
    {"border-bottom-width", P<CssLen>([](ComputedStyle& style, CssLen len) {
         style.borderWidthBottom = len.inPixels_;
     })},
    {"border-left-width", P<CssLen>([](ComputedStyle& style, CssLen len) {
         style.borderWidthLeft = len.inPixels_;
     })},
    {"border-right-width", P<CssLen>([](ComputedStyle& style, CssLen len) {
         style.borderWidthRight = len.inPixels_;
     })},
    {"padding-top", P<CssLen>([](ComputedStyle& style, CssLen len) {
         style.paddingTop = len.inPixels_;
     })},
    {"padding-bottom", P<CssLen>([](ComputedStyle& style, CssLen len) {
         style.paddingBottom = len.inPixels_;
     })},
    {"padding-left", P<CssLen>([](ComputedStyle& style, CssLen len) {
         style.paddingLeft = len.inPixels_;
     })},
    {"padding-right", P<CssLen>([](ComputedStyle& style, CssLen len) {
         style.paddingRight = len.inPixels_;
     })},
};

Parser<CssSource> cssFileParser() {
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
            bool parserFound = false;
            for (const auto& parser : cssValueParsers) {
                if (parser.first == decl.property_
                    && parser.second.parameterCount_ == decl.values_.size())
                {
                    decl.apply_ = parser.second.parser_(decl.values_);
                    parserFound = true;
                    break;
                }
            }
            if (!parserFound) {
                stringstream msg;
                msg << "no parser found for \""
                    << decl.property_ << "\" with "
                    << decl.values_.size() << " argument(s).";
                throw std::invalid_argument(msg.str());
            }
            return decl;
    }};
    Parser<CssSelector> parseSelector = {
        [nextToken] (SourceStream& source) {
            source.skipWhitespace();
            vector<string> selectorRaw;
            while (!source.startswith("{") && !source.startswith("}") && !source.startswith(",")) {
                string tok = nextToken(source);
                selectorRaw.push_back(tok);
                if (source.skipWhitespace() > 0) {
                    selectorRaw.push_back(" ");
                }
            }
            // remove whitespace tokens at end:
            while (!selectorRaw.empty() && *(selectorRaw.rbegin()) == " ") {
                selectorRaw.pop_back();
            }
            if (selectorRaw.empty()) {
                source.raise("a selector must not be empty");
            }
            CssSelector selector;
            selector.content_.reserve(selectorRaw.size());
            for (const auto& tok : selectorRaw) {
                selector.content_.push_back(Converter<CssName>::parse(tok));
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
            CssSource file;
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



void CssSource::print(std::ostream& out) const
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
                out << Converter<CssName>::str(s);
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
            Parser<CssSource> parser = cssFileParser();
            CssSource file;
            auto stream = SourceStream::fromString(cssSource);
            try {
                file = parser(stream);
            } catch (const SourceStream::Error& error) {
                output.error() << error.str();
                return 1;
            } catch (const std::exception& exc) {
                auto sourceError = stream.constructErrorObject(exc.what());
                output.error() << sourceError.str();
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


template<> CssName Converter<CssName>::parse(const string& source)
{
    for (const auto& row : CssName::specialNames) {
        if (row.second == source) {
            CssName name;
            name.special_ = row.first;
            return name;
        }
    }
    // else:
    if (source.empty()) {
        throw std::invalid_argument("A class name must not be empty");
    }
    CssName name;
    name.custom_ = source;
    return name;
}

template<> string Converter<CssName>::str(CssName payload)
{
    if (payload.custom_.empty()) {
        for (const auto& row : CssName::specialNames) {
            if (row.first == payload.special_) {
                return row.second;
            }
        }
    }
    return payload.custom_;
}

template<> void Converter<CssName>::complete(Completion&, CssName const*)
{
}

template<> CssLen Converter<CssLen>::parse(const string& source)
{
    if (source.size() < 2 || source.substr(source.size() - 2) != "px") {
        throw std::invalid_argument("length must be of the format \"<n>px\", e.g. 4px");
    }
    int pxInt = Converter<int>::parse(source.substr(0, source.size() - 2));
    CssLen len;
    len.inPixels_ = static_cast<short>(pxInt);
    return len;
}

template<> std::string Converter<CssLen>::str(CssLen payload)
{
    return Converter<int>::str(payload.inPixels_) + "px";
}

template<> void Converter<CssLen>::complete(Completion&, CssLen const*)
{
}

template<> CssSource Converter<CssSource>::parse(const string& source)
{
    Parser<CssSource> parser = cssFileParser();
    auto stream = SourceStream::fromString(source);
    SourceStream::Error parseError;
    try {
        return parser(stream);
    } catch (const SourceStream::Error& error) {
        parseError = error;
    } catch (const std::exception& exc) {
        parseError = stream.constructErrorObject(exc.what());
    }
    throw std::invalid_argument(parseError.str());
}

template<> std::string Converter<CssSource>::str(CssSource payload)
{
    stringstream buf;
    payload.print(buf);
    return buf.str();
}

template<> void Converter<CssSource>::complete(Completion&, CssSource const*)
{
}
