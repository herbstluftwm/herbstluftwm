#pragma once

#include "object.h"

class KeyManager : public Object {
public:
    int addKeybindCommand(Input input, Output output);
    int listKeybindsCommand(Output output);
    int removeKeybindCommand(Input input, Output output);
};
