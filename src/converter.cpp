#include "converter.h"

#include "completion.h"

using std::string;

template<> void Converter<bool>::complete(Completion& complete, bool const* relativeTo)
{
    complete.full({ "on", "off", "true", "false" });
    if (relativeTo != nullptr) {
        complete.full("toggle");
    }
}

void completeFull(Completion &complete, string s)
{
    complete.full(s);
}

template<>
void Converter<Direction>::complete(Completion& complete, const Direction* relativeTo)
{
    complete.full({"up", "down", "left", "right"});
}

static std::vector <std::pair<DirectionDepth, std::string>>
DirectionDepthMap = {
    { DirectionDepth::Frame, "frame" },
    { DirectionDepth::Visible, "visible" },
    { DirectionDepth::Tabs, "tabs" },
    { DirectionDepth::All, "all" }
};

template<>
DirectionDepth Converter<DirectionDepth>::parse(const std::string &payload) {
    for (auto pair : DirectionDepthMap) {
        if (pair.second == payload) {
            return pair.first;
        }
    }
    throw std::invalid_argument("Invalid direction level \"" + payload + "\"");
}

template<>
std::string Converter<DirectionDepth>::str(const DirectionDepth d)
{
    for (auto pair : DirectionDepthMap) {
        if (pair.first == d) {
            return pair.second;
        }
    }
    throw std::logic_error("not_reached");
}

template<>
unsigned long Converter<unsigned long>::parse(const string& source)
{
    size_t pos = 0;
    long value = std::stol(source, &pos);
    if (pos < source.size()) {
        throw std::invalid_argument("unparsable suffix: "
                                    + source.substr(pos));
    }
    if (value < 0) {
        throw std::invalid_argument("negative number is out of range");
    } else {
        return (unsigned long)(value);
    }
}
