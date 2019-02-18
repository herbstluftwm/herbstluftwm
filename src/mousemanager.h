#pragma once

#include <X11/Xlib.h>

#include "object.h"

class MouseManager : public Object {
public:
    MouseManager();
    ~MouseManager();

private:
    Cursor cursor;
};
