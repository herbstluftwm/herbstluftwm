
#include <string>
#include <sstream>
#include "clientobject.h"

using namespace herbstluft;

ClientObject::ClientObject(Window w)
    : HSClient(w)
{
    std::stringstream tmp;
    tmp << "0x" << std::hex << w;
    auto window_str = tmp.str();
    name_ = window_str;
}
ClientObject::~ClientObject() {
}
