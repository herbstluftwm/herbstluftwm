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
