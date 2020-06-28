#include "frameparser.h"

#include <algorithm>
#include <sstream>

#include "arglist.h"
#include "globals.h"
#include "fixprecdec.h"
#include "hlwmcommon.h"
#include "root.h"
#include "x11-types.h"

using std::dynamic_pointer_cast;
using std::make_pair;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::stringstream;
using std::vector;

shared_ptr<RawFrameLeaf> RawFrameLeaf::isLeaf() {
    return dynamic_pointer_cast<RawFrameLeaf>(shared_from_this());
}

shared_ptr<RawFrameSplit> RawFrameSplit::isSplit() {
    return dynamic_pointer_cast<RawFrameSplit>(shared_from_this());
}


class ParsingException : public std::exception {
public:
    ParsingException(FrameParser::Token tok, string message)
        : token_(tok), message_(message) {
    }
    FrameParser::Token token_;
    string message_;
};


FrameParser::FrameParser(string buf) {
    eofToken = make_pair(buf.size(), "");
    parse(buf);
}

void FrameParser::parse(string buf) {
    Tokens tokens = tokenize(buf);
    try {
        nextToken = tokens.begin();
        endToken = tokens.end();
        root_ = buildTree();
        if (nextToken != endToken) {
            throw ParsingException(*nextToken,
                                   "Layout description too long");
        }
    } catch (const ParsingException& e) {
        error_ = make_shared<pair<Token,string>>(
            make_pair(e.token_, e.message_));
    }
}

bool FrameParser::contained_in(char c, string s) {
    return s.find(c) != string::npos;
}

FrameParser::Tokens FrameParser::tokenize(string buf) {
    Tokens tokens;
    size_t pos = 0;
    while (pos < buf.size()) {
        string whitespace = "\n\r ";
        string parentheses = "()";
        if (contained_in(buf[pos], whitespace)) {
            // skip whitespace
            pos++;
        } else if (contained_in(buf[pos], parentheses)) {
            // parentheses are always single character tokens
            tokens.push_back(make_pair(pos, buf.substr(pos, 1)));
            pos++;
        }
        else {
            // everything else is a token until the next whitespace character
            size_t beg = pos;
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
        throw ParsingException(eofToken, "Expected argument list");
    }
    ArgList args (nextToken->second, ':');
    if (isSplit) {
        // Construct a RawFrameSplit
        auto node = make_shared<RawFrameSplit>();
        string alignName, fractionStr, selectionStr;
        args >> alignName >> fractionStr >> selectionStr;
        if (!args || !args.empty()) {
            stringstream message;
            args.reset();
            message << "Expected 3 arguments but got " << args.size();
            throw ParsingException(*nextToken, message.str());
        }
        try {
            node->align_ = Converter<SplitAlign>::parse(alignName);
            FixPrecDec fraction = Converter<FixPrecDec>::parse(fractionStr);
            if (fraction < FRAME_MIN_FRACTION
                || fraction > (FixPrecDec::fromInteger(1) - FRAME_MIN_FRACTION))
            {
                stringstream message;
                message << "Fraction must be between "
                        <<  FRAME_MIN_FRACTION.str() << " and "
                        << (FixPrecDec::fromInteger(1) - FRAME_MIN_FRACTION).str()
                        << " but actually is " << Converter<FixPrecDec>::str(fraction);
                throw std::invalid_argument(message.str());
            }
            node->fraction_ = fraction;
            node->selection_ = std::stoi(selectionStr);
            if (node->selection_ != 0 && node->selection_ != 1) {
                throw std::invalid_argument("selection must be 0 or 1");
            }
        } catch (const std::exception& e) {
            throw ParsingException(*nextToken, e.what());
        }
        nextToken++;

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
        string layoutName, selectionStr;
        args >> layoutName >> selectionStr;
        if (!args || !args.empty()) {
            stringstream message;
            args.reset();
            message << "Expected 2 arguments but got " << args.size();
            throw ParsingException(*nextToken, message.str());
        }
        try {
            node->layout = Converter<LayoutAlgorithm>::parse(layoutName);
            node->selection = std::stoi(selectionStr);
            if (node->selection < 0) {
                throw std::invalid_argument("selection must not be negative.");
            }
        } catch (const std::exception& e) {
            throw ParsingException(*nextToken, e.what());
        }
        nextToken++;
        // Construct a RawFrameLeaf
        while (nextToken != endToken && nextToken->second != ")") {
            Window winid;
            try {
                // if the window id is syntactically wrong, then throw an error
                winid = Converter<WindowID>::parse(nextToken->second);
            } catch (const std::exception& e) {
                throw ParsingException(*nextToken, "not a valid window id");
            }
            // if the window id is unknown, then just print a warning
            Client* client = Root::common().client(winid);
            if (client) {
                node->clients.push_back(client);
            } else {
                unknownWindowIDs_.push_back(make_pair(*nextToken, winid));
            }
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
        stringstream message;
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
        auto tok = (nextToken == endToken) ? eofToken : *nextToken;
        throw ParsingException(tok, message.str());
    }
}
