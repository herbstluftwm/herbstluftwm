#pragma once

#include <map>
#include <memory>
#include <string>

#include "attribute_.h"
#include "converter.h"
#include "entity.h"

class FontData;
class XConnection;

/**
 * @brief An object of this class holds a font.
 * It is essentially a shared pointer to a FontData object
 */
class HSFont
{
public:
    static HSFont fromStr(const std::string& source);
    std::string str() { return source_; }
    bool operator==(const HSFont& o) {
        return source_ == o.source_;
    }
    bool operator!=(const HSFont& o) {
        return source_ != o.source_;
    }
    FontData& data() const { return *fontData_; }
private:
    HSFont();
    std::string source_;
    std::shared_ptr<FontData> fontData_;
    static std::map<std::string, std::weak_ptr<FontData>> s_fontDescriptionToData;
};


template<> HSFont Converter<HSFont>::parse(const std::string& source);
template<> std::string Converter<HSFont>::str(HSFont payload);

template<>
inline Type Attribute_<HSFont>::staticType() { return Type::FONT; }
