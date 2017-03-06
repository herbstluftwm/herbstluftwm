#ifndef __HLWM_BY_NAME_H_
#define __HLWM_BY_NAME_H_

#include <memory>
#include <map>
#include "object.h"
#include "hook.h"

class ByName : public Object, public Hook {
public:
    // ByName is an object making each child of 'parent' addressible by its
    // name
    ByName(Object& parent);
    ~ByName();

    void childAdded(Object* parent, std::string child_name);
    void childRemoved(Object* parent, std::string child_name);
    void attributeChanged(Object* child, std::string attribute_name);
private:
    Object& parent;
    // for each child, remember it's last name
    std::map<Object*, std::string> last_name;
};

#endif

