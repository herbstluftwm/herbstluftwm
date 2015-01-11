#include "x11-types.h"

#include <iostream>
#include <iomanip>

namespace herbstluft {

std::ostream& operator<< (std::ostream& stream, const Rectangle& rect) {
    stream
        << rect.width << "x" << rect.height
        << std::showpos
        << rect.x << rect.y
        << std::noshowpos;
    return stream;
}

}
