#include "types.h"

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
    long value = std::stol(source);
    if (value < 0) {
        throw std::invalid_argument("negative number is out of range");
    } else {
        return (unsigned long)(value);
    }
}
