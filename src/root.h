#ifndef ROOT_H
#define ROOT_H

#include "directory.h"

// new object tree root.
namespace herbstluft {

class HookManager;
class ClientManager;

class Root : public Directory {
public:
    static std::shared_ptr<Root> create();
    static void destroy();
    static std::shared_ptr<Root> get() { return root_; }

    Root();

    /* convenience methods */
    std::shared_ptr<HookManager> hooks();

private:

    static std::shared_ptr<Root> root_;
};

}

#endif // ROOT_H
