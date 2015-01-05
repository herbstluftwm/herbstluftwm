#include "attribute.h"
#include "object.h"

namespace herbstluft {

std::string Attribute::read() const
{
    if (owner_)
        return owner_->read(name_);
    return {};
}

void Attribute::write(const std::string &value)
{
    if (owner_)
        owner_->write(name_, value);
}

void Action::trigger(const std::string &value)
{
    if (owner_)
        owner_->trigger(name_, value);
}

}
