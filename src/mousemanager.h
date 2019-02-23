#pragma once

#include <X11/Xlib.h>
#include <list>

#include "mouse.h"
#include "object.h"

class MouseManager : public Object {
public:
    MouseManager();
    ~MouseManager();

    int addMouseBindCommand(Input input, Output output);

    //! Currently defined mouse bindings (TODO: make this private as soon as possible)
    std::list<MouseBinding> binds;

private:
    Cursor cursor;
};
