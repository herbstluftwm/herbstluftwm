#pragma once

#include <X11/X.h>
#include <list>

#include "mouse.h"
#include "object.h"

class Completion;

class MouseManager : public Object {
public:
    MouseManager();
    ~MouseManager();

    int addMouseBindCommand(Input input, Output output);

    void addMouseBindCompletion(Completion &complete);

    //! Currently defined mouse bindings (TODO: make this private as soon as possible)
    std::list<MouseBinding> binds;

private:
    Cursor cursor;
};
