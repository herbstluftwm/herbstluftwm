
#include <string>
#include <sstream>
#include "clientobject.h"


ClientObject::ClientObject(Window w, bool already_visible)
    : HSClient(w, already_visible)
    , window_id_str("winid", "")
{
    // setup object name
    std::stringstream tmp;
    tmp << "0x" << std::hex << w;
    auto window_str = tmp.str();
    window_id_str = window_str;
    wireAttributes({
        &keymask_,
        &pseudotile_,
    });
}
ClientObject::~ClientObject() {
}
