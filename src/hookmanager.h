#ifndef HOOKMANAGER_H
#define HOOKMANAGER_H

#include <string>

#include "arglist.h"
#include "attribute.h"
#include "object.h"
#include "types.h"

class HookManager : public Object
{
public:
    HookManager();

    // custom handling (hook names contain '.', they never have children)
    void ls(Path path, Output out) override;

    void add(const std::string &path);
    void remove(const std::string &path);

    void trigger(const std::string &action, ArgList args) override;

private:
    Action add_;
    Action remove_;
};


#endif // HOOKMANAGER_H
