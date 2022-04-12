#include "css.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <tuple>

#include "argparse.h"
#include "globals.h"

using std::endl;
using std::function;
using std::make_pair;
using std::pair;
using std::string;
using std::stringstream;
using std::vector;

class SourceStream {
private:
    string buf;
    size_t pos = 0;

public:
    static
    SourceStream fromString(string source) {
        SourceStream ss;
        ss.buf = source;
        ss.pos = 0;
        return ss;
    };
    class Error : public std::exception {
    public:
        size_t line_ = 0;
        size_t column_ = 0;
        string message_;
        string str() const {
            stringstream ss;
            ss << "line " << (line_ + 1)
               << " column " << (column_ + 1)
               << ": " << message_;
            return ss.str();
        }
    };

    //! whitespace characters
    static constexpr auto whitespace = " \n\t\r";
    inline static bool contains(char const* characterList, char member) {
        while (characterList[0]) {
            if (member == characterList[0]) {
                return true;
            }
            characterList++;
        }
        return false;
    }

    inline bool startswith(char const* prefix) const {
        size_t idx = 0;
        while (prefix[idx]) {
            if (pos + idx >= buf.size()) {
                return false;
            }
            if (prefix[idx] != buf[pos + idx]) {
                return false;
            }
            idx++;
        }
        return true;
    }

    //! try to consume the given prefix from the input stream
    inline bool tryConsume(char const* prefix) {
        if (startswith(prefix)) {
            pos += strlen(prefix);
            return true;
        } else {
            return false;
        }
    }

    /**
     * @brief skip whitespace characters and comments
     * @return the number of whitespace characters skipped
     */
    size_t skipWhitespace() {
        size_t skipCount = 0;
        while (pos < buf.size()) {
            if (contains(whitespace, buf[pos])) {
                pos++;
                skipCount++;
            } else if (tryConsume("/*")) {
                while (!tryConsume("*/")) {
                    pos++;
                    if (isEOF()) {
                        // multiline comments may not be ended with EOF
                        expectedButGot("*/");
                    }
                }
            } else if (tryConsume("//")) {
                while (!tryConsume("\n") && !isEOF()) {
                    pos++;
                }
            } else {
                break;
            }
        }
        return skipCount;
    }

    /**
     * @brief return the current position in the source stream
     * @return tuple of line/column (0-indexed)
     */
    pair<size_t,size_t> sourcePosition() const {
        size_t line = 0;
        size_t idx = 0;
        size_t column = pos - idx;
        while (idx <= pos && idx < buf.size()) {
            if (buf[idx] == '\n') {
                line++;
                column = pos - (idx + 1);
            }
            idx++;
        }
        return make_pair(line, column);
    }

    void raise(const char* message) {
        auto sourcePos = sourcePosition();
        Error err;
        err.line_ = sourcePos.first;
        err.column_ = sourcePos.second;
        err.message_ = message;
        throw err;
    }

    void expectedButGot(const char* expected) {
        stringstream message;
        message << "Expected " << expected << " but got ";
        if (pos >= buf.size()) {
            message << "EOF";
        } else {
            auto preview = buf.substr(pos, 20);
            std::replace(preview.begin(), preview.end(), '\n', ' '); // replace all '\n' with ' '
            message << "\"" << preview << "...\"";
        }
        raise(message.str().c_str());
    }

    inline void consumeOrException(char const* prefix) {
        if (!tryConsume(prefix)) {
            expectedButGot(prefix);
        }
    }

    bool isEOF() {
        return pos >= buf.size();
    }

    /**
     * @brief return the next token, which consists of either a single special character
     * or a sequence of non-special characters
     * @param special the special characters
     * @return
     */
    string nextToken(const char* special) {
        size_t token_pos = pos;
        while (pos < buf.size()) {
            if (token_pos == pos && contains(special, buf[pos])) {
                // if the token starts with a special character,
                // then the resulting token only contains this character
                pos++;
                break;
            }
            if (contains(special, buf[pos]) || contains(whitespace, buf[pos])) {
                // end the token before special characters or whitespace
                break;
            }
            // otherwise, include the current character in the token.
            pos++;
        }
        string token = buf.substr(token_pos, pos - token_pos);
        if (token.empty()) {
            expectedButGot("any kind of token");
        }
        return token;
    }
};

template<typename Result>
class Parser {
public:
    Parser(function<Result(SourceStream& source)> r) : run_(r) {};
    bool isApplicable(SourceStream& source) const {
        if (source.isEOF()) {
            return false;
        }
        if (isApplicable_) {
            return isApplicable_(source);
        }
        return true;
    }
    Result operator()(SourceStream& source) const {
        return run_(source);
    }


    /**
     * @brief construct a modified parser that is only applicable if
     * the stream does not start with the given forbidden prefix
     */
    Parser<Result> notApplicableFor(string forbiddenPrefix) const {
        auto oldGuard = isApplicable_;
        auto newGuard = [oldGuard, forbiddenPrefix](SourceStream& source) {
            if (source.startswith(forbiddenPrefix.c_str())) {
                return false;
            } else if (oldGuard) {
                return oldGuard(source);
            } else {
                return true;
            }
        };
        Parser<Result> newParser = *this;
        newParser.isApplicable_ = newGuard;
        return newParser;
    }

    /**
     * @brief parser constructor: iteration with separater between and
     * possibly after elements
     */
    Parser<vector<Result>> sepEndBy(char separator) const
    {
        return { [&,separator](SourceStream& source) {
            vector<Result> result;
            const char separatorStr[] = {separator, '\0'};
            while (true) {
                if (!this->isApplicable(source)) {
                    break;
                }
                result.push_back(this->run_(source));
                if (!source.tryConsume(separatorStr)) {
                    break;
                }
                source.skipWhitespace();
            }
            return result;
        }};
    }
    /**
     * @brief parser constructor: iteration of this parser.
     */
    Parser<vector<Result>> many() const
    {
        return {
            [&](SourceStream& source) {
                auto& elementParser = *this;
                vector<Result> result;
                while (true) {
                    if (!elementParser.isApplicable(source)) {
                        break;
                    }
                    result.push_back(elementParser(source));
                }
                return result;
        }};
    }

private:
    //! the actual parser:
    function<Result(SourceStream& source)> run_;
    //! a function that checks whether we are allowed to run the parser
    function<bool(SourceStream& source)> isApplicable_ = {};
};


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

