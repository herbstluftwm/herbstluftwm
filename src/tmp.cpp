
#include "tmp.h"
#include "utils.h"
#include "ipc-protocol.h"

using namespace std;

Tmp::Tmp(Root* root_)
  : root(root_)
{
}

int Tmp::mktemp(Input input, Output output) {
    std::string cmd, type, identifier;
    if (!input.read({ &cmd, &type, &identifier })) {
        return HERBST_NEED_MORE_ARGS;
    }
    output << "to be implemented!" << endl;
    return 12;
}

