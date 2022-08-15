#ifndef SINGLETON_H
#define SINGLETON_H

template<const char[] Name>
class Singleton {
public:
    bool operator==(const Inherit& other) { return true; }
};

template<>
inline std::string Converter<Inherit>::str(Inherit payload) { return ""; }
template<>
inline Inherit Converter<Inherit>::parse(const std::string &payload) {
    if (payload.empty()) {
        return {};
    }
    throw std::invalid_argument("Use an empty string for inheriting the value.");
}
template<> void Converter<Inherit>::complete(Completion& complete, Inherit const* relativeTo);

#endif // SINGLETON_H
