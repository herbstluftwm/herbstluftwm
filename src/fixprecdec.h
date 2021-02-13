#ifndef FIXPRECDEC_H
#define FIXPRECDEC_H

#include "attribute_.h"
#include "types.h"

/**
 * @brief A class for fixed precision decimals encoded in the type int.
 * The unit is globally fixed to FixPrecDec::unit_;
 */
class FixPrecDec
{
public:
    int value_;
    static const int unit_; //! the number one encoded as a FixPrecDec

    std::string str() const;

    bool operator<(double other);
    bool operator>(double other);

    bool operator<(FixPrecDec other) {
        return value_ < other.value_;
    }

    bool operator>(FixPrecDec other) {
        return value_ > other.value_;
    }

    static FixPrecDec fromInteger(int integer) {
        return integer * unit_;
    }

    FixPrecDec operator+(FixPrecDec other) {
        return value_ + other.value_;
    }
    FixPrecDec operator-(FixPrecDec other) {
        return value_ - other.value_;
    }
    FixPrecDec operator/(int divisor) {
        return value_ / divisor;
    }
    static FixPrecDec raw(int value) { return value; }
    static FixPrecDec approxFrac(int nominator, int denominator);
private:
    FixPrecDec(int value) : value_(value) {}
};

template<> FixPrecDec Converter<FixPrecDec>::parse(const std::string& source);
template<> std::string Converter<FixPrecDec>::str(FixPrecDec payload);

template<>
inline Type Attribute_<FixPrecDec>::staticType() { return Type::DECIMAL; }


#endif // FIXPRECDEC_H
