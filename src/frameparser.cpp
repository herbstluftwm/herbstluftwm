#include "frameparser.h"

#include <algorithm>
#include <sstream>

using std::shared_ptr;
using std::string;
using std::vector;

FrameParser::FrameParser(string buf) {
}

void FrameParser::parse(string buf) {
    Tokens tokens = tokenize(buf);
    root_ = buildTree();
}

bool FrameParser::contained_in(char c, std::string s) {
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
            tokens.push_back(std::make_pair(pos, buf.substr(pos, 1)));
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
            tokens.push_back(std::make_pair(beg, buf.substr(beg, pos - beg)));
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
        auto node = std::make_shared<RawFrameSplit>();
        nextToken++; // TODO: parse the member data
        auto a = buildTree();
        auto b = buildTree();
        nodeUntyped = node;
    } else {
        auto node = std::make_shared<RawFrameLeaf>();
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

void FrameParser::expectTokens(std::vector<std::string> tokens) {
    if (std::find(tokens.begin(), tokens.end(), nextToken->second) == tokens.end()) {
        std::stringstream message;
        message << "Invalid token \"" << nextToken->second << "\". Expected ";
        if (tokens.size() == 1) {
            message << "\"" << tokens[0] << "\"";
        } else {
            message << "one of:";
            for (auto& t : tokens) {
                message << " \"" << t << "\"";
            }
        }
        message << ".";
        // TODO: throw exception
    }
}
