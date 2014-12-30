#include "attribute.h"
#include "object.h"

namespace herbstluft {

std::string Attribute::read() const
{
    if (auto o = owner_.lock())
        return o->read(name_);
    return {};
}

void Attribute::write(const std::string &value)
{
    if (auto o = owner_.lock())
        o->write(name_, value);
}

void Action::trigger(const std::string &value)
{
    if (auto o = owner_.lock())
        o->trigger(name_, value);
}

}
