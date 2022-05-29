#ifndef EITHER_H
#define EITHER_H

#include <functional>
#include <string>
#include <type_traits>

#include "converter.h"

/**
 * The Either<A,B> class holds either something of type
 * A or of type B. This is essentially what is called
 * a 'tagged union' or 'variant' in C++17.
 * This only works properly if A and B are sufficiently
 * incompatible and the C++ function overloading finds the right
 * constructor.
 */
template<typename A, typename B>
class Either {
public:
    /** @brief perform a case distinction and call either a
     *  handler for the type A or for the type B. Both
     *  handlers have the same return type X, so in any of the
     *  two cases, the case distinction returns something of type X.
     *
     *  (Up to the side effects of the handlers, this is essentially
     *  what's also called the "universal property of the coproduct")
     * @param the handler for type A
     * @param the handler for type B
     * @return the return value of the appropriate handler
     */
    template<typename X> // using X = std::string;
    X cases(std::function<X(const A&)> onA, std::function<X(const B&)> onB) const {
        if (isA_) {
            return onA(data_.a_);
        } else {
            return onB(data_.b_);
        }
    }

    // since 'void' is not a typename, we need to handle this explicitly:
    void cases(std::function<void(const A&)> onA, std::function<void(const B&)> onB) const {
        if (isA_) {
            return onA(data_.a_);
        } else {
            return onB(data_.b_);
        }
    }
    Either(const A& a)
        : data_(a), isA_(true)
    {}
    Either(const B& b)
        : data_(b), isA_(false)
    {}
    Either(const Either<A,B>& other)
    {
        isA_ = other.isA_;
        if (other.isA_) {
            new (&data_.a_) A(other.data_.a_);
        } else {
            new (&data_.b_) B(other.data_.b_);
        }
    }
    ~Either() {
        if (isA_) {
            data_.a_.~A();
        } else {
            data_.b_.~B();
        }
    }
    bool isLeft() const {
        return isA_;
    }
    bool isRight() const {
        return !isA_;
    }
    void operator=(const Either<A,B>& other) {
        if (isA_ == other.isA_) {
            if (isA_) {
                data_.a_ = other.data_.a_;
            } else {
                data_.b_ = other.data_.b_;
            }
        } else {
            // destructor
            if (isA_) {
                data_.a_.~A();
            } else {
                data_.b_.~B();
            }
            // constructor
            isA_ = other.isA_;
            if (other.isA_) {
                new (&data_.a_) A(other.data_.a_);
            } else {
                new (&data_.b_) B(other.data_.b_);
            }
        }
    }
    bool operator==(const Either<A,B>& other) {
        if (isA_ && other.isA_) {
            return data_.a_ == other.data_.a_;
        }
        if (!isA_ && !other.isA_) {
            return data_.b_ == other.data_.b_;
        }
        return false;
    }
    bool operator!=(const Either<A,B>& other) {
        return !(*this == other);
    }

    // convert Either<A,B> to type 'B'
    B rightOr(const B& onA) const {
        if (isA_) {
            return onA;
        } else {
            return data_.b_;
        }
    }

private:
    Either() = delete;
    union UnionAB {
    public:
        UnionAB() {
            // A/B constructors are called by ~Either;
        }
        UnionAB(A a) : a_(a) {}
        UnionAB(B b) : b_(b) {}
        ~UnionAB() {
            // A/B destructors are called by ~Either;
        }
        A a_;
        B b_;
    };
    UnionAB data_;
    bool isA_;
};

class Completion;

/**
 * The Converter for Either<A,B> combines the Converter of A and B.
 * In the parsing, priority is given to A, i.e. if the parser for A
 * succeeds it is not even attempted to parse the given source
 * string to something of type B. All other functions are symmetric
 * in A and B.
 */
template <typename A, typename B>
class Converter<Either<A,B>, typename std::enable_if< std::true_type::value >::type> {
public:
    static std::string str(const Either<A,B>& payload) {
        return payload.template cases<std::string>(Converter<A>::str, Converter<B>::str);
    }
    static Either<A,B> parse(const std::string& source) {
        try {
            return Converter<A>::parse(source);
        }  catch (const std::exception& e1) {
            try {
                return Converter<B>::parse(source);
            }  catch (const std::exception& e2) {
                throw std::invalid_argument(std::string(e1.what()) + "; or: " + e2.what());
            }
        }
    }
    static Either<A,B> parse(const std::string& source, const Either<A,B>&) {
        return parse(source);
    };
    static void complete(Completion& complete, Either<A,B> const*) {
        Converter<A>::complete(complete, nullptr);
        Converter<B>::complete(complete, nullptr);
    }
    static void complete(Completion& completionObject) {
        complete(completionObject, nullptr);
    }
};

#endif // EITHER_H
