#ifndef ROOT_H
#define ROOT_H

#include "utils.h"
#include "object.h"

// new object tree root.
namespace herbstluft {

class Attribute;
class HookManager;
class ClientManager;

class Root : public Object {
public:
    static std::shared_ptr<Root> create();
    static void destroy();
    static std::shared_ptr<Root> get() { return root_; }

    // constructor creates top-level objects
    Root();

    Attribute* getAttribute(std::string path, Output output);

    /* convenience methods */
    static std::shared_ptr<HookManager> hooks();
    static std::shared_ptr<ClientManager> clients();

    /* external interface */
    static int cmd_ls(Input in, Output out);
    // find an attribute deep in the object tree.
    // on failure, the error message is printed to output and NULL
    // is returned
    static int cmd_get_attr(Input args, Output output);
    static int cmd_attr(Input args, Output output);


private:

    static std::shared_ptr<Root> root_;
};

int print_object_tree_command(ArgList args, Output output);

}

#endif // ROOT_H
