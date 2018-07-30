#include "attribute.h"
#include "object.h"


/**
 * In the given range of values, find the current value of the attribute and set the
 * attribute to the next value in the range. If the value is not found or last
 * in the range, then the attribute is set to the first element in the range.
 */
std::string Attribute::cycleValue(std::vector<std::string>::const_iterator begin,
                                  std::vector<std::string>::const_iterator end)
{
    if (begin == end) {
        return {};
    }
    std::string cur_value = str();
    for (auto it = begin; it != end; ++it) {
        if (*it == cur_value) {
            // if the value is found
            ++it;
            if (it != end) {
                // and if the next is still in range
                // change the value
                return change(*it);
            } else {
                break;
            }
        }
    }
    // if the value is not found or was the last element
    // set the value to the first in the range
    return change(*begin);
}

void Attribute::detachFromOwner() {
    owner_->removeAttribute(this);
    owner_ = NULL;
}
