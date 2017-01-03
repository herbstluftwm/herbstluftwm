#ifndef __HLWM_BY_NAME_H_
#define __HLWM_BY_NAME_H_

#include "object.h"

class ByName : public Object {
public:
    // ByName is an object making each child of 'parent' addressible by its
    // name
    ByName(Object* parent);
};

#endif

