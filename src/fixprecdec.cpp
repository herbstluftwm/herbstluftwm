#include "fixprecdec.h"

#include <cctype>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

using std::string;
using std::stringstream;
using std::vector;

const int FixPrecDec::unit_ = 10000;

template<>
FixPrecDec Converter<FixPrecDec>::parse(const string& source)
{
    vector<string> parts = ArgList::split(source, '.');
    if (parts.size() > 2) {
        throw std::invalid_argument("A decimal must have at most one \'.\'");
    }
    if (parts[0].empty()) {
        throw std::invalid_argument("There must be at least one digit before \'.\'");
    }
    // check the signum from the string, because the signum may get lost if
    // parts[0] is "-0"
    int signum = (parts[0][0] == '-') ? -1 : 1;
    int value = Converter<int>::parse(parts[0]) * FixPrecDec::unit_;
    if (parts.size() == 1) {
        // no decimal separator, so nothing more to parse
        return FixPrecDec::raw(value);
    }
    int remaining_unit = FixPrecDec::unit_;
    for (char ch : parts[1]) {
        if (!isdigit(ch)) {
            throw std::invalid_argument("After \'.\' only digits may follow");
        }
        remaining_unit /= 10;
        // we can add digits after the decimal separator, but we need
        // to subtract if the entire number is negative
        value += signum * (ch - '0') * remaining_unit;
        if (remaining_unit == 0) {
            break;
        }
    }
    return FixPrecDec::raw(value);
}

template<>
string Converter<FixPrecDec>::str(FixPrecDec payload)
{
    return payload.str();
}

string FixPrecDec::str() const
{
    int v = value_;
    stringstream fmt;
    if (v < 0) {
        fmt << "-";
        v *= -1;
    }
    // from now on, we can act as if the value is positive (this makes modulo easier)
    fmt << (v / unit_);
    v %= unit_;
    if (v != 0) {
        fmt << ".";
        int remaining_unit = unit_;
        while (v != 0) {
            remaining_unit /= 10;
            fmt << (v / remaining_unit);
            v %= remaining_unit;
        }
    }
    return fmt.str();
}

bool FixPrecDec::operator<(double other)
{
    return value_ < other * unit_;
}

bool FixPrecDec::operator>(double other)
{
    return value_ > other * unit_;
}

/**
 * @brief Construct a FixPrecDec that roughly is the given fraction
 * @param nominator
 * @param denominator
 * @return
 */
FixPrecDec FixPrecDec::approxFrac(int nominator, int denominator)
{
    return unit_ * nominator / denominator;
}
