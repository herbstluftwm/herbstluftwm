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
    /** Construct a regex. The case where source is "" means
     * that the regex is inactive, see isUnset().
     *
     * may throw std::invalid_argument exception
     */
    static RegexStr fromStr(const std::string& source);
    std::string str() const { return source_; }
    /** returns when the source string is empty, respectivelly the regex is 'unset'
     */
    bool empty() const { return source_.empty(); }
    /** returns whether two regexes have the same source. In particular,
     * two RegexStr objects are equal if they are both unset.
     */
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
inline Type Attribute_<RegexStr>::staticType() { return Type::REGEX; }

#endif // REGEXSTR_H
