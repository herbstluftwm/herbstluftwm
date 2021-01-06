#include "font.h"

#include <map>

#include "fontdata.h"

using std::make_shared;
using std::shared_ptr;
using std::string;
using std::weak_ptr;


/**
 * @brief This map caches loaded fonts, such that whenever the same
 * font description is given in multiple attributes (which will most likely
 * happen due to the proxy-attributes in the theme objects), then all these
 * HSFont-objects have a shared pointer to the same FontData objects. When
 * there are no more HSFont objcts pointing to a particular FontData object
 * then this object is automatically deallocated, because this map here only
 * carries a weak pointer.
 */
std::map<string, weak_ptr<FontData>> HSFont::s_fontDescriptionToData;

/**
 * @brief Create a FontData object. If the font description
 * is invalid or the font is not found, an exception is thrown.
 * @param The font description as the user would enter it
 * @return
 */
HSFont HSFont::fromStr(const string& source)
{
    auto it = s_fontDescriptionToData.find(source);
    shared_ptr<FontData> data;
    if (it != s_fontDescriptionToData.end() && !it->second.expired()) {
        data = it->second.lock();
    } else {
        data = make_shared<FontData>();
        data->initFromStr(source); // possibly throws an exception
        s_fontDescriptionToData[source] = data;
    }
    HSFont font;
    font.source_ = source;
    font.fontData_ = data;
    return font;
}

HSFont::HSFont()
{
}

template<>
HSFont Converter<HSFont>::parse(const string& source)
{
    return HSFont::fromStr(source);
}

template<>
string Converter<HSFont>::str(HSFont payload)
{
    return payload.str();
}
