#ifndef __HERBSTLUFT_ROOTCOMMANDS_H_
#define __HERBSTLUFT_ROOTCOMMANDS_H_

/** commands that don't belong to a particular object
 * but modify the global state */

#include "utils.h"
#include <functional>

class Root;
class Attribute;

class RootCommands {
public:
    // this class collects high-level commands that don't need any internal
    // structures but just the object tree as the user sees it.
    // Hence, this does not inherit from Object and is not exposed to the user as an object.
    RootCommands(Root* root);
    int substitute_cmd(Input input, Output output);
    int sprintf_cmd(Input input, Output output);
    int new_attr_cmd(Input input, Output output);
    int remove_attr_cmd(Input input, Output output);
    int compare_cmd(Input input, Output output);
    static Attribute* newAttributeWithType(std::string typestr, std::string attr_name, Output output);
private:
    Root* root;
};


#endif
