#ifndef REGEXSTR_H
#define REGEXSTR_H

#include <regex>

#include "attribute_.h"
#include "types.h"

/** wrapper class for extended regexes that remembers
 * its source string
 */
class RegexStr
{
public:
    RegexStr();
    //! may throw std::invalid_argument exception
    static RegexStr fromStr(const std::string& source);
    std::string str() const { return source_; }
    /** returns when the source is the empty string, respectivelly is 'unset'
     * (has nothing to do with the language of the regex being empty)
     */
    bool empty() const { return source_.empty(); }
    bool operator==(const RegexStr& other) const;
    bool operator!=(const RegexStr& o) const { return ! operator==(o); }
    bool matches(const std::string& str) const;
private:
    std::string source_;
    std::regex regex_;
};

template<> RegexStr Converter<RegexStr>::parse(const std::string& source);
template<> std::string Converter<RegexStr>::str(RegexStr payload);

template<>
inline Type Attribute_<RegexStr>::staticType() { return Type::ATTRIBUTE_REGEX; }

#endif // REGEXSTR_H
