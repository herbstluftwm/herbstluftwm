#include "tmp.h"

#include "attribute.h"
#include "command.h"
#include "completion.h"
#include "ipc-protocol.h"
#include "metacommands.h"

using std::string;
using std::to_string;

int Tmp::mktemp(Input input, Output output) {
    string type, identifier;
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
    Attribute* a = MetaCommands::newAttributeWithType(type, attr_name, output);
    if (!a) {
        return HERBST_INVALID_ARGUMENT;
    }
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

void Tmp::mktempComplete(Completion& complete)
{
    if (complete == 0) {
        MetaCommands::completeAttributeType(complete);
    } else if (complete == 1) {
        // no completion for the identifier
    } else {
        complete.full(complete[1]);
        complete.completeCommands(2);
    }
}
