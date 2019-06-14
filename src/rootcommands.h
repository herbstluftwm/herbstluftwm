#ifndef __HERBSTLUFT_ROOTCOMMANDS_H_
#define __HERBSTLUFT_ROOTCOMMANDS_H_

/** commands that don't belong to a particular object
 * but modify the global state */

#include <functional>
#include <memory>
#include <vector>

#include "types.h"

class Root;
class Attribute;
class Completion;

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
    void get_attr_complete(Completion& complete);
    int set_attr_cmd(Input args, Output output);
    void set_attr_complete(Completion& complete);
    int attr_cmd(Input args, Output output);
    void attr_complete(Completion& complete);
    int print_object_tree_command(Input args, Output output);
    void print_object_tree_complete(Completion& complete);

    int substitute_cmd(Input input, Output output);
    int sprintf_cmd(Input input, Output output);
    int new_attr_cmd(Input input, Output output);
    int remove_attr_cmd(Input input, Output output);
    void remove_attr_complete(Completion& complete);
    int compare_cmd(Input input, Output output);
    static Attribute* newAttributeWithType(std::string typestr, std::string attr_name, Output output);
    void completeObjectPath(Completion& complete, bool attributes = false,
                            std::function<bool(Attribute*)> attributeFilter = {});
    void completeAttributePath(Completion& complete);

    int echoCommand(Input input, Output output);
    void echoCompletion(Completion& ) {}; // no completion
private:
    Root* root;
    std::vector<std::unique_ptr<Attribute>> userAttributes_;
};


#endif
