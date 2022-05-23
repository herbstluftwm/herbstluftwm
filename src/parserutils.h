#pragma once

/** A small generic tokenizer and parser, mainly for CSS parsing.
 *  This header must not be included in another header file.
 */
#include <algorithm>
#include <cstring>
#include <functional>
#include <sstream>
#include <vector>

class SourceStream {
private:
    std::string buf;
    size_t pos = 0;

public:
    static
    SourceStream fromString(std::string source) {
        SourceStream ss;
        ss.buf = source;
        ss.pos = 0;
        return ss;
    }
    class Error : public std::exception {
    public:
        size_t line_ = 0;
        size_t column_ = 0;
        std::string message_;
        std::string str() const {
            std::stringstream ss;
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
    std::pair<size_t,size_t> sourcePosition() const {
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
        return std::make_pair(line, column);
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
        std::stringstream message;
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
    std::string nextToken(const char* special) {
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
        std::string token = buf.substr(token_pos, pos - token_pos);
        if (token.empty()) {
            expectedButGot("any kind of token");
        }
        return token;
    }
};

template<typename Result>
class Parser {
public:
    Parser(const std::function<Result(SourceStream& source)>& r) : run_(r) {}
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
    Parser<Result> notApplicableFor(std::string forbiddenPrefix) const {
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
    Parser<std::vector<Result>> sepEndBy(char separator) const
    {
        return { [&,separator](SourceStream& source) {
            std::vector<Result> result;
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
    Parser<std::vector<Result>> many() const
    {
        return {
            [&](SourceStream& source) {
                auto& elementParser = *this;
                std::vector<Result> result;
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
    std::function<Result(SourceStream& source)> run_;
    //! a function that checks whether we are allowed to run the parser
    std::function<bool(SourceStream& source)> isApplicable_ = {};
};


