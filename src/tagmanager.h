#ifndef __HLWM_TAGMANAGER_H_
#define __HLWM_TAGMANAGER_H_

#include "tag.h"
#include "childbyindex.h"


class TagManager : public ChildByIndex<HSTag> {
public:
    TagManager();
private:
    Ptr(Object) by_name;
};


#endif
