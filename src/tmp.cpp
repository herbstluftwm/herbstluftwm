#include "tmp.h"

#include "attribute.h"
#include "command.h"
#include "ipc-protocol.h"
#include "rootcommands.h"

int Tmp::mktemp(Input input, Output output) {
    std::string type, identifier;
    if (!(input >> type >> identifier)) {
        return HERBST_NEED_MORE_ARGS;
    }
    number_active++;
    number_total %= 1000;
    number_total++;
    // get some kind of uniqueness to reveal mistakes more easily
    string attr_name =
        "tmp_" + to_string(number_active)
        + "_" +  to_string(number_total);
    Attribute* a = RootCommands::newAttributeWithType(type, attr_name, output);
    if (!a) return HERBST_INVALID_ARGUMENT;
    addAttribute(a);
    string path = string(TMP_OBJECT_PATH) + "." + attr_name;

    auto carryover = input.fromHere();
    carryover.replace(identifier, path);
    int retval = Commands::call(carryover, output);

    a->detachFromOwner();
    delete a;
    number_active--;
    return retval;
}

