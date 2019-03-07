#ifndef __HLWM_BY_NAME_H_
#define __HLWM_BY_NAME_H_

#include <map>
#include <string>

#include "hook.h"
#include "object.h"

class ByName : public Object, public Hook {
public:
    // ByName is an object making each child of 'parent' addressible by its
    // name
    ByName(Object& parent);
    ~ByName() override;

    void childAdded(Object* parent, std::string child_name) override;
    void childRemoved(Object* parent, std::string child_name) override;
    void attributeChanged(Object* child, std::string attribute_name) override;
private:
    Object& parent;
    // for each child, remember it's last name
    std::map<Object*, std::string> last_name;
};

#endif

