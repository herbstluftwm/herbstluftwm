
#include "tmp.h"
#include "utils.h"
#include "rootcommands.h"
#include "command.h"
#include "ipc-protocol.h"
#include "attribute.h"

using namespace std;

Tmp::Tmp()
    : number_active(0)
    , number_total(0)
{
}

int Tmp::mktemp(Input input, Output output) {
    std::string cmd, type, identifier;
    if (!input.read({ &cmd, &type, &identifier })) {
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
    int retval = Commands::call(input.replace(identifier, path), output);
    a->detachFromOwner();
    delete a;
    number_active--;
    return retval;
}

