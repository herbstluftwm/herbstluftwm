#include "frameparser.h"

using std::string;
using std::vector;

FrameParser::FrameParser(string buf) {
}

void FrameParser::parse(string buf) {
    Tokens tokens = tokenize(buf);
    for (auto& token : tokens) {
        int pos = token.first;
        const string& s = token.second;
    }
}

bool FrameParser::contained_in(char c, std::string s) {
    return s.find(c) != string::npos;
}

FrameParser::Tokens FrameParser::tokenize(string buf) {
    Tokens tokens;
    int pos = 0;
    while (pos < buf.size()) {
        char c = buf[pos];
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

