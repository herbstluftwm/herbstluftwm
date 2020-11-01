#ifndef HOOKMANAGER_H
#define HOOKMANAGER_H

#include "attribute.h"
#include "object.h"

class HookManager : public Object
{
public:
    HookManager();

    void add(const std::string &path);
    void remove(const std::string &path);

private:
    Action add_;
    Action remove_;
};


#endif // HOOKMANAGER_H
