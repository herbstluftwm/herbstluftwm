#ifndef CSSNAME_H
#define CSSNAME_H

#include <memory>
#include <string>
#include <vector>

#include "converter.h"

class CssNameData;

class CssName {
public:
    enum class Builtin {
        /* CSS Combinators */
        child,
        descendant,
        has_class,
        pseudo_class,
        first_child,
        last_child,
        adjacent_sibling,
        any,
        LAST_COMBINATOR = any,
        /* built in names */
        tabbar,
        tab,
        no_tabs,
        one_tab,
        multiple_tabs,
        bar,
        notabs,
        focus,
        normal,
        urgent,
        minimal,
        fullscreen,
        floating,
        tiling,
        client_decoration,
        /* insert above, so that this stays last */
        client_content,
        LAST = client_content,
    };
    CssName(Builtin builtin) {
        index_ = static_cast<size_t>(builtin);
    }
    CssName(const std::string& name);
    CssName(const CssName& other) = default;

    bool isCombinator() const;
    bool isBinaryOperator() const;
    inline static bool isBuiltin(size_t index) {
        return index <= static_cast<size_t>(Builtin::LAST);
    }
    bool isBuiltin() const {
        return isBuiltin(index_);
    }
    bool operator==(const CssName& other) const {
        return index_ == other.index_;
    }
    bool operator==(Builtin builtin) const {
        return index_ == static_cast<size_t>(builtin);
    }
    inline size_t index() {
        return index_;
    }
private:
    size_t index_;
    // extra data, only set for non-builtin names:
    std::shared_ptr<CssNameData> data_ = {};
};


template<> CssName Converter<CssName>::parse(const std::string& source);
template<> std::string Converter<CssName>::str(CssName payload);
template<> void Converter<CssName>::complete(Completion& complete, CssName const* relativeTo);


class CssNameSet {
public:
    CssNameSet() = default;
    CssNameSet(std::initializer_list<std::pair<CssName, bool>> classes);
    void setEnabled(CssName className, bool enabled);
    bool contains(CssName className) const;
private:
    unsigned long long int names_ = 0; // at least 64 bits
    static constexpr size_t namesLength_ = sizeof(CssNameSet::names_) * 8;
    std::vector<bool> moreNames_; // anything not fitting in names_
    std::vector<CssName> customNames_; // names that are not builtIn
};




#endif // CSSNAME_H
