#ifndef HOOKMANAGER_H
#define HOOKMANAGER_H

#include "object.h"

namespace herbstluft {

class HookManager : public Object
{
public:
    HookManager();

    void add(const std::string &path);
    void remove(const std::string &path);

    void trigger(const std::string &action, ArgList args);

private:
    Action add_;
    Action remove_;
};

}

#endif // HOOKMANAGER_H
