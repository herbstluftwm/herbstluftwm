
#include <string>
#include <sstream>
#include "clientobject.h"

using namespace herbstluft;

ClientObject::ClientObject(Window w, bool already_visible)
    : HSClient(w, already_visible)
{
    // setup object name
    std::stringstream tmp;
    tmp << "0x" << std::hex << w;
    auto window_str = tmp.str();
    name_ = window_str;
    wireAttributes({
        &keymask_,
        &pseudotile_,
    });
}
ClientObject::~ClientObject() {
}
