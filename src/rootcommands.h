#ifndef __HERBSTLUFT_ROOTCOMMANDS_H_
#define __HERBSTLUFT_ROOTCOMMANDS_H_

/** commands that don't belong to a particular object
 * but modify the global state */

#include "types.h"
#include <functional>

class Root;
class Attribute;

class RootCommands {
public:
    // this class collects high-level commands that don't need any internal
    // structures but just the object tree as the user sees it.
    // Hence, this does not inherit from Object and is not exposed to the user as an object.
    RootCommands(Root* root);

    Attribute* getAttribute(std::string path, Output output);

    /* external interface */
    // find an attribute deep in the object tree.
    // on failure, the error message is printed to output and NULL
    // is returned
    int get_attr_cmd(Input args, Output output);
    int set_attr_cmd(Input args, Output output);
    int attr_cmd(Input args, Output output);
    int print_object_tree_command(Input args, Output output);

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
