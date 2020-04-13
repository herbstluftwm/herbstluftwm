#include "regexstr.h"

using std::regex;
using std::string;

RegexStr::RegexStr()
{
}

RegexStr RegexStr::fromStr(const string &source)
{
    RegexStr r;
    r.source_ = source;
    try {
        r.regex_ = regex(source, regex::extended);
    }  catch (const std::exception& e) {
        throw std::invalid_argument(e.what());
    }
    return r;
}

bool RegexStr::operator==(const RegexStr& other) const
{
    return source_ == other.source_;
}

bool RegexStr::matches(const string& str) const
{
    return regex_match(str, regex_);
}

template<> RegexStr Converter<RegexStr>::parse(const string& source) {
    return RegexStr::fromStr(source);
}
template<> string Converter<RegexStr>::str(RegexStr payload) {
    return payload.str();
}
