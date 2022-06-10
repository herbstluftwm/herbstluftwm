#include "css.h"

#include <algorithm>
#include <iostream> // TODO: remove again
#include <cstring>
#include <sstream>
#include <tuple>

#include "argparse.h"
#include "globals.h"
#include "parserutils.h"

using std::endl;
using std::function;
using std::make_pair;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::stringstream;
using std::vector;
using std::weak_ptr;

const vector<pair<CssName::Builtin, string>> CssName::specialNames =
{
    { CssName::Builtin::child, ">" },
    { CssName::Builtin::has_class, "." },
    { CssName::Builtin::pseudo_class, ":" },
    { CssName::Builtin::descendant, " " },
    { CssName::Builtin::adjacent_sibling, "+" },
    { CssName::Builtin::any, "*" },
    { CssName::Builtin::tabbar, "tabbar" },
    { CssName::Builtin::tab, "tab" },
    { CssName::Builtin::notabs, "notabs" },
    { CssName::Builtin::no_tabs, "no-tabs" },
    { CssName::Builtin::one_tab, "one-tab" },
    { CssName::Builtin::multiple_tabs, "multiple-tabs" },
    { CssName::Builtin::bar, "bar" },
    { CssName::Builtin::client_content, "client-content" },
    { CssName::Builtin::first_child, "first-child" },
    { CssName::Builtin::last_child, "last-child" },
    { CssName::Builtin::window, "client-decoration" },
    { CssName::Builtin::minimal, "minimal" },
    { CssName::Builtin::fullscreen, "fullscreen" },
    { CssName::Builtin::urgent, "urgent" },
    { CssName::Builtin::focus, "focus" },
    { CssName::Builtin::normal, "normal" },
    { CssName::Builtin::floating, "floating" },
    { CssName::Builtin::tiling, "tiling" },
};

class CssFileParser {
public:
    function<string(SourceStream&)> nextToken_;
    Parser<CssDeclaration> parseDecl_;
    Parser<CssSelector> parseSelector_;
    Parser<CssRuleSet> parseRuleSet_;
    Parser<CssSource> parseFile_;

    CssFileParser()
    {
        auto nextToken = [](SourceStream& source) -> string {
            return source.nextToken("/*{}>~,;+:.");
        };
        Parser<CssDeclaration> parseDecl {[nextToken] (SourceStream& source) -> CssDeclaration {
                CssDeclaration decl;
                decl.property_ = nextToken(source);
                CssValueParser prop = CssValueParser::find(decl.property_);
                source.skipWhitespace();
                source.consumeOrException(":");
                source.skipWhitespace();
                while (!source.startswith(";") && !source.startswith("}")) {
                    decl.values_.push_back(nextToken(source));
                    source.skipWhitespace();
                }
                source.skipWhitespace();
                decl.apply_ = prop.parse(decl.values_);
                return decl;
        }};
        Parser<CssSelector> parseSelector {[nextToken] (SourceStream& source) -> CssSelector {
            source.skipWhitespace();
            vector<string> selectorRaw;
            while (!source.isEOF() && !source.startswith("{") && !source.startswith("}") && !source.startswith(",")) {
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
            for (auto it = selectorRaw.begin(); it != selectorRaw.end(); it++) {
                CssName parsed = Converter<CssName>::parse(*it);
                CssName whitespace{CssName::Builtin::descendant};
                if (parsed.isBinaryOperator()) {
                    // drop whitespace before binary operator
                    while (!selector.content_.empty()
                           && *(selector.content_.rbegin()) == whitespace) {
                        selector.content_.pop_back();
                    }
                    // drop whitespace after binary operator
                    while (it + 1 != selectorRaw.end() && *(it + 1) == " ") {
                        it++;
                    }
                }
                selector.content_.push_back(parsed);
            }
            return selector;
        }};
        Parser<CssRuleSet> parseRuleSet {
            [parseSelector, parseDecl] (SourceStream& source) -> CssRuleSet {
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
        Parser<CssSource> parseFile {[parseRuleSet] (SourceStream& source) {
            CssSource file;
            source.skipWhitespace();
            file.content_ = parseRuleSet.many()(source);
            source.skipWhitespace();
            if (!source.isEOF()) {
                source.expectedButGot("EOF");
            }
            file.recomputeSortedSelectors();
            return file;
        }};
        nextToken_ = nextToken;
        parseDecl_ = parseDecl;
        parseSelector_ = parseSelector;
        parseRuleSet_ = parseRuleSet;
        parseFile_ = parseFile;
    }
};

class DummyTree : public DomTree, public std::enable_shared_from_this<DummyTree> {
public:
    using Ptr = shared_ptr<DummyTree>;
    vector<string> classes_ = {};
    CssNameSet classesHashed_ = {};
    vector<DummyTree::Ptr> children_ = {};
    weak_ptr<DummyTree> parent_ = {};
    size_t indexInParent_ = 0;
    static DummyTree::Ptr parse(SourceStream& source) {
        source.consumeOrException("(");
        DummyTree::Ptr tree = make_shared<DummyTree>();
        source.skipWhitespace();
        while (!source.isEOF() && !source.startswith(")")) {
            if (source.startswith("(")) {
                auto child = parse(source);
                child->parent_ = tree;
                child->indexInParent_ = tree->children_.size();
                tree->children_.push_back(child);
            } else {
                tree->classes_.push_back(source.nextToken("() "));
            }
            source.skipWhitespace();
        }
        for (const auto& name : tree->classes_) {
            tree->classesHashed_.setEnabled(name, true);
        }
        source.consumeOrException(")");
        return tree;
    }
    DummyTree::Ptr byTreeIndex(const vector<int>& treeIndex, size_t pos = 0) {
        if (pos >= treeIndex.size()) {
            return shared_from_this();
        }
        int idx = treeIndex[pos];
        if (idx < 0 || idx >= static_cast<int>(children_.size())) {
            return {};
        }
        return children_[idx]->byTreeIndex(treeIndex, pos + 1);
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
            child->print(output, indent + 1);
        }
        output << ")";
    }
    vector<int> treeIndex() const {
        auto parent = parent_.lock();
        if (parent) {
            vector<int> index = parent->treeIndex();
            index.push_back(static_cast<int>(indexInParent_));
            return index;
        } else {
            return {};
        }
    }

    void recurse(function<void(DummyTree::Ptr)> body) {
        body(shared_from_this());
        for (const auto& child : children_) {
            child->recurse(body);
        }
    }

    const DomTree* parent() const override {
        auto ptr = parent_.lock();
        if (ptr) {
            return ptr.get();
        }
        return nullptr;
    }
    const DomTree* nthChild(size_t idx) const override {
        if (idx < children_.size()) {
            return children_[idx].get();
        }
        return nullptr;
    }
    const DomTree* leftSibling() const override {
        auto ptr = parent_.lock();
        if (indexInParent_ > 0 && ptr) {
            return ptr->nthChild(indexInParent_ - 1);
        }
        return nullptr;
    }
    bool hasClass(const CssName& className) const override {
        return classesHashed_.contains(className);
    }
    size_t childCount() const override {
        return children_.size();
    }
};

template<>
DummyTree::Ptr Converter<DummyTree::Ptr>::parse(const string& source) {
    try {
        SourceStream stream = SourceStream::fromString(source);
        stream.skipWhitespace();
        DummyTree::Ptr tree = DummyTree::parse(stream);
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
string Converter<DummyTree::Ptr>::str(DummyTree::Ptr payload) {
    stringstream output;

    payload->print(output);
    return output.str();
}

template<>
void Converter<DummyTree::Ptr>::complete(Completion& complete, const DummyTree::Ptr*) {
}

template<>
vector<int> Converter<vector<int>>::parse(const string& source) {
    vector<int> result;
    for (const auto& i :  ArgList::split(source, ' ')) {
        result.push_back(Converter<int>::parse(i));
    }
    return result;
}

template<>
string Converter<vector<int>>::str(vector<int> payload) {
    stringstream output;
    bool first = true;
    for (int i : payload) {
        if (!first) {
            output << " ";
        }
        first = false;
        output << i;
    }
    return output.str();
}

template<>
void Converter<vector<int>>::complete(Completion& complete, const vector<int>*) {
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
                if (s.isBinaryOperator()) {
                    out << " ";
                }
                out << Converter<CssName>::str(s);
                if (s.isBinaryOperator()) {
                    out << " ";
                }
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

shared_ptr<BoxStyle> CssSource::computeStyle(DomTree* element) const
{
    shared_ptr<BoxStyle> style = make_shared<BoxStyle>();
    computeStyle(element, style);
    return style;
}

void CssSource::computeStyle(DomTree* element, std::shared_ptr<BoxStyle> target) const
{
    for (const auto& selectorIdx : sortedSelectors_) {
        const auto& block = content_[selectorIdx.second.indexInContent_];
        const auto& selector = block.selectors_[selectorIdx.second.indexInSelectors_];
        if (selector.matches(element)) {
            for (const auto& property : block.declarations_) {
                property.apply_(*target);
            }
        }
    }
    std::cerr << "{\n";
    for (const auto& it : target->changedProperties()) {
        std::cerr << "  " << it.first << ": " << it.second << ";\n";
    }
    std::cerr << "}\n";
}

void CssSource::recomputeSortedSelectors()
{
    size_t selectorCount = 0;
    for (const auto& block : content_) {
        selectorCount += block.selectors_.size();
    }
    sortedSelectors_.clear();
    sortedSelectors_.reserve(selectorCount);
    SelectorIndex selIdx;
    for (const auto& block : content_) {
        selIdx.indexInSelectors_ = 0;
        for (const auto& selector : block.selectors_) {
            sortedSelectors_.push_back(make_pair(selector.specifity(), selIdx));
            selIdx.indexInSelectors_++;
        }
        selIdx.indexInContent_++;
    }
    std::stable_sort(
              sortedSelectors_.begin(),
              sortedSelectors_.end(),
              [](const Specifity2Idx& item1, const Specifity2Idx& item2) -> bool {
        return item1.first < item2.first;
    });
}

void debugCssCommand(CallOrComplete invoc)
{
    string cssSource;
    bool print = false, printTree = false;
    DummyTree::Ptr tree;
    string cssSelectorStr;
    bool treeIndexPresent = false;
    vector<int> treeIndex = {};
    ArgParse ap;
    ap.mandatory(cssSource);
    ap.flags({
        {"--print-css", &print },
        {"--tree=", tree },
        {"--print-tree", &printTree },
        {"--query-tree-indices=", cssSelectorStr },
        {"--compute-style=", treeIndex, &treeIndexPresent},
    });
    ap.command(invoc,
        [&] (Output output) {
            auto parser = CssFileParser();
            CssSource file;
            bool error = false;
            parser.parseFile_.oneShot(cssSource)
                  .cases(
                        [&output,&error](const SourceStream::ErrorData& err) {
                output.error() << err.str();
                error = true;
            },
            [&file](const CssSource& data) {
                file = data;
            });
            if (error) {
                return 1;
            }

            if (print) {
                file.print(output.output());
            }
            if (printTree) {
                output << Converter<DummyTree::Ptr>::str(tree) << endl;
            }
            if (!cssSelectorStr.empty()) {
                if (!tree) {
                    output.error() << "selector queries requires a tree";
                    return 1;
                }
                CssSelector selector;
                parser.parseSelector_.oneShot(cssSelectorStr)
                        .cases([&output, &error](const SourceStream::ErrorData& err) {
                    output.error() << err.str();
                    error = true;
                }, [&selector](const CssSelector res) {
                    selector = res;
                });
                tree->recurse([&output, &selector](DummyTree::Ptr node){
                    if (selector.matches(node.get())) {
                        output << "match: " << Converter<vector<int>>::str(node->treeIndex()) << "\n";
                    }
                });
            }
            if (treeIndexPresent) {
                // compute the style for the given node
                if (!tree) {
                    output.error() << "--compute-style requires a tree";
                    return 1;
                }
                DummyTree::Ptr nodeOfInterest = tree->byTreeIndex(treeIndex);
                if (!nodeOfInterest) {
                    output.error() << "invalid tree index.";
                    return 1;
                }
                std::map<string,string> properties;
                auto boxStyle = file.computeStyle(nodeOfInterest.get());
                for (const auto& it : boxStyle->changedProperties()) {
                    output << it.first << ": " << it.second << ";\n";
                }
            }
            return 0;
    });
}


template<> CssName Converter<CssName>::parse(const string& source)
{
    for (const auto& row : CssName::specialNames) {
        if (row.second == source) {
            return row.first;
        }
    }
    // else:
    if (source.empty()) {
        throw std::invalid_argument("A class name must not be empty");
    }
    return source;
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

template<> CssSource Converter<CssSource>::parse(const string& source)
{
    auto parser = CssFileParser();
    auto stream = SourceStream::fromString(source);
    SourceStream::Error parseError;
    try {
        return parser.parseFile_(stream);
    } catch (const SourceStream::Error& error) {
        parseError = error;
    } catch (const std::exception& exc) {
        parseError = stream.constructErrorObject(exc.what());
    }
    throw std::invalid_argument(parseError.str());
}

template<> string Converter<CssSource>::str(CssSource payload)
{
    stringstream buf;
    payload.print(buf);
    return buf.str();
}

template<> void Converter<CssSource>::complete(Completion&, CssSource const*)
{
}

bool CssSelector::matches(const DomTree* element) const
{
    return matches(element, content_.size());
}

CssSelector::Specifity CssSelector::specifity() const
{
    Specifity spec;
    int skipItems = 0;
    for (const auto& item : content_) {
        while (skipItems > 0) {
            skipItems--;
            continue;
        }
        if (item == CssName::Builtin::has_class) {
            spec.classSelectors++;
            skipItems++;
            continue;
        }
        if (item == CssName::Builtin::pseudo_class) {
            spec.classSelectors++;
            skipItems++;
            continue;
        }
    }
    return spec;
}

bool CssSelector::matches(const DomTree* element, size_t prefixLen) const
{
    if (prefixLen >= content_.size()) {
        prefixLen = content_.size();
    }
    if (prefixLen == 0 || content_.empty()) {
        return true;
    }
    if (element == nullptr) {
        return false;
    }
    const CssName& current = content_[prefixLen - 1];
    if (current.isBuiltin()) {
        switch (current.special_) {
        case CssName::Builtin::child:
            return matches(element->parent(), prefixLen - 1);
        case CssName::Builtin::adjacent_sibling:
            return matches(element->leftSibling(), prefixLen - 1);
        case CssName::Builtin::descendant:
            {
                const DomTree* parent = element->parent();
                while (parent) {
                    // try to match the selector
                    if (matches(parent, prefixLen - 1)) {
                        return true;
                    }
                    // otherwise, go one level further up
                    parent = parent->parent();
                }
                return false;
            }
        case CssName::Builtin::any:
            return matches(element, prefixLen - 1);
        default:
            // we have an ordinary token in the selector,
            // this could be a selector for element type, class, or id.
            if (prefixLen >= 2) {
                CssName::Builtin previous =
                        content_[prefixLen - 2].isBuiltin()
                        ? content_[prefixLen - 2].special_
                        : CssName::Builtin::any;
                switch (previous) {
                case CssName::Builtin::has_class:
                    return element->hasClass(current)
                            && matches(element, prefixLen - 2);
                case CssName::Builtin::pseudo_class:
                {
                    const DomTree* parent = element->parent();
                    switch (current.special_) {
                        case CssName::Builtin::first_child:
                            return parent &&
                                    parent->nthChild(0) == element &&
                                    matches(element, prefixLen - 2);
                        case CssName::Builtin::last_child:
                            return parent &&
                                    parent->nthChild(parent->childCount() - 1) == element &&
                                    matches(element, prefixLen - 2);
                        default:
                            return false;
                    }
                }
                default:
                    // check element type: not implemented yet so always false
                    return false;
                }
            }
        }
    }
    return false;
}

bool CssName::isCombinator() const
{
    return custom_.empty() && special_ <= Builtin::LAST_COMBINATOR;
}

/*** whether this is a binary operator, i.e. something that consumes
 * whitespace before and after
 */
bool CssName::isBinaryOperator() const
{
    return custom_.empty()
            && (special_ == Builtin::adjacent_sibling
                || special_ == Builtin::child);
}


void CssNameSet::setEnabled(std::initializer_list<pair<CssName, bool> > classes)
{
    for (const auto& item : classes) {
        setEnabled(item.first, item.second);
    }
}

bool CssNameSet::contains(CssName className) const
{
    if (className.isBuiltin()) {
        return names_ & (1ull << static_cast<unsigned long long>(className.special_));
    } else {
        return false;
    }
}

void CssNameSet::setEnabled(CssName className, bool enabled)
{
    if (className.isBuiltin()) {
        if (enabled) {
            names_ |= (1ull << static_cast<unsigned long long>(className.special_));
        } else {
            names_ &= ~(1ull << static_cast<unsigned long long>(className.special_));
        }
    }
}

CssRuleSet::CssRuleSet(std::initializer_list<CssSelector> selectors,
                       std::initializer_list<CssDeclaration> declarations)
    : selectors_(selectors)
    , declarations_(declarations)
{
}
