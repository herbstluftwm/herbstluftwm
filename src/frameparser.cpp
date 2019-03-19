#include "frameparser.h"

#include "arglist.h"

#include <algorithm>
#include <sstream>

using std::make_pair;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;

FrameParser::FrameParser(string buf) {
}

class ParsingException : public std::exception {
public:
    ParsingException(int pos, string message)
        : pos_(pos), message_(message) {
    }
    int pos_; //! the position or -1 for "end of string"
    string message_;
};

void FrameParser::parse(string buf) {
    Tokens tokens = tokenize(buf);
    try {
        root_ = buildTree();
    } catch (ParsingException& e) {
        int pos = (e.pos_ < 0) ? buf.size() : e.pos_;
        error_ = make_pair(pos, e.message_);
    }
}

bool FrameParser::contained_in(char c, string s) {
    return s.find(c) != string::npos;
}

FrameParser::Tokens FrameParser::tokenize(string buf) {
    Tokens tokens;
    int pos = 0;
    while (pos < buf.size()) {
        string whitespace = "\n\r ";
        string parentheses = "()";
        if (contained_in(buf[pos], whitespace)) {
            // skip whitespace
            pos++;
        } else if (contained_in(buf[pos], parentheses)) {
            // parentheses are always single character tokens
            tokens.push_back(make_pair(pos, buf.substr(pos, 1)));
        }
        else {
            // everything else is a token until the next whitespace character
            int beg = pos;
            while (pos < buf.size()
                   && !contained_in(buf[pos], whitespace)
                   && !contained_in(buf[pos], parentheses))
            {
                // scan until whitespace or next token
                pos++;
            }
            tokens.push_back(make_pair(beg, buf.substr(beg, pos - beg)));
        }
    }
    return tokens;
}

shared_ptr<RawFrameNode> FrameParser::buildTree() {
    expectTokens({ "(" });
    nextToken++;
    expectTokens({ "split", "clients" });
    bool isSplit = nextToken->second == "split";
    nextToken++;
    shared_ptr<RawFrameNode> nodeUntyped = nullptr;
    // in both cases, the next token is a list of ':'-separated arguments
    if (nextToken == endToken) {
        throw ParsingException(-1, "Expected argument list");
    }
    auto args = ArgList::split(nextToken->second, ':');
    nextToken++;
    if (isSplit) {
        // Construct a RawFrameSplit
        auto node = make_shared<RawFrameSplit>();
        expectTokens({ "(", ")" });
        if (nextToken->second == "(") {
            node->a_ = buildTree();
        }
        expectTokens({ "(", ")" });
        if (nextToken->second == "(") {
            node->b_ = buildTree();
        }
        nodeUntyped = node;
    } else {
        auto node = make_shared<RawFrameLeaf>();
        // Construct a RawFrameLeaf
        while (nextToken != endToken && nextToken->second != ")") {
            string winid_str = nextToken->second;
            nextToken++;
        }
        nodeUntyped = node;
    }
    expectTokens({ ")" });
    nextToken++;
    return nodeUntyped;
}

void FrameParser::expectTokens(vector<string> tokens) {
    if (nextToken == endToken ||
        std::find(tokens.begin(), tokens.end(), nextToken->second) == tokens.end()) {
        std::stringstream message;
        if (nextToken == endToken) {
            message << "Unexpected end of input.";
        } else {
            message << "Invalid token \"" << nextToken->second << "\".";
        }
        message << " Expected ";
        if (tokens.size() == 1) {
            message << "\"" << tokens[0] << "\"";
        } else {
            message << "one of:";
            for (auto& t : tokens) {
                message << " \"" << t << "\"";
            }
        }
        message << ".";
        int pos = (nextToken == endToken) ? -1 : nextToken->first;
        throw ParsingException(pos, message.str());
    }
}
