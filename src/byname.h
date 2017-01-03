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

    void childAdded(std::shared_ptr<Object> parent, std::string child_name);
    void childRemoved(std::shared_ptr<Object> parent, std::string child_name);
    void attributeChanged(std::shared_ptr<Object> child, std::string attribute_name);
private:
    Object& parent;
    // for each child, remember it's last name
    std::map<std::shared_ptr<Object>, std::string> last_name;
};

#endif

