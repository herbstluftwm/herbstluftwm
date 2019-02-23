#pragma once

#include <X11/Xlib.h>
#include <vector>

#include "mouse.h"
#include "object.h"

class MouseManager : public Object {
public:
    MouseManager();
    ~MouseManager();

    //! Currently defined mouse bindings (TODO: make this private as soon as possible)
    std::vector<MouseBinding> binds;

private:
    Cursor cursor;
};
