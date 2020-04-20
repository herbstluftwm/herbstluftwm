#include "regexstr.h"

using std::string;

RegexStr::RegexStr()
{
}

RegexStr RegexStr::fromStr(const string& source)
{
    RegexStr r;
    r.source_ = source;
    // An empty regex is not allowed in the POSIX grammar
    // as one can infer from the grammar at
    // https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap09.html#tag_09_05_03
    // => So we must not compile "" to a regex
    if (!source.empty()) {
        try {
            r.regex_ = std::regex(source, std::regex::extended);
        }  catch (const std::exception& e) {
            throw std::invalid_argument(e.what());
        }
    }
    return r;
}

bool RegexStr::operator==(const RegexStr& other) const
{
    return source_ == other.source_;
}

bool RegexStr::matches(const string& str) const
{
    if (source_.empty()) {
        return false;
    } else {
        return std::regex_match(str, regex_);
    }
}

template<> RegexStr Converter<RegexStr>::parse(const string& source) {
    return RegexStr::fromStr(source);
}
template<> string Converter<RegexStr>::str(RegexStr payload) {
    return payload.str();
}
