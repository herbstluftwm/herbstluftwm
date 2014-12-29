#include "attribute.h"
#include "object.h"

namespace herbstluft {

bool Attribute::readable() {
    if (auto o = owner_.lock())
        return o->readable(name_);
    return false;
}

std::string Attribute::read()
{
    if (auto o = owner_.lock())
        return o->read(name_);
    return {};
}

bool Attribute::writeable()
{
    if (auto o = owner_.lock())
        return o->writeable(name_);
    return false;
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
