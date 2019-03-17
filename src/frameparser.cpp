#include "frameparser.h"

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
    int pos_;
    string message_;
};

void FrameParser::parse(string buf) {
    Tokens tokens = tokenize(buf);
    try {
        root_ = buildTree();
    } catch (ParsingException& e) {
        error_ = make_pair(e.pos_, e.message_);
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
    if (isSplit) {
        // Construct a RawFrameSplit
        auto node = make_shared<RawFrameSplit>();
        nextToken++; // TODO: parse the member data
        auto a = buildTree();
        auto b = buildTree();
        nodeUntyped = node;
    } else {
        auto node = make_shared<RawFrameLeaf>();
        // Construct a RawFrameLeaf
        nextToken++; // TODO: parse the member data
        while (nextToken->second != ")") {
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
        throw ParsingException(nextToken->first, message.str());
    }
}
