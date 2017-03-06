#ifndef ROOT_H
#define ROOT_H

#include "utils.h"
#include "object.h"
#include "child.h"

// new object tree root.

class Attribute;
class TagManager;
class HookManager;
class ClientManager;
class MonitorManager;
class Theme;

class Root : public Object {
public:
    //static std::shared_ptr<Root> create();
    //static void destroy();
    static std::shared_ptr<Root> get() { return root_; }
    static void setRoot(const std::shared_ptr<Root>& r) { root_ = r; }

    // constructor creates top-level objects
    Root();
    ~Root();

    Attribute* getAttribute(std::string path, Output output);

    /* external interface */
    // find an attribute deep in the object tree.
    // on failure, the error message is printed to output and NULL
    // is returned
    int cmd_get_attr(Input args, Output output);
    int cmd_attr(Input args, Output output);

    Child_<ClientManager> clients;
    Child_<TagManager> tags;
    Child_<MonitorManager> monitors;
    Child_<HookManager> hooks;
    Child_<Theme> theme;

private:

    static std::shared_ptr<Root> root_;
};

int print_object_tree_command(ArgList args, Output output);


#endif // ROOT_H
