#ifndef __HLWM_TAGMANAGER_H_
#define __HLWM_TAGMANAGER_H_

#include "tag.h"
#include "childbyindex.h"
#include "byname.h"


class TagManager : public ChildByIndex<HSTag> {
public:
    TagManager();
private:
    ByName by_name;
};

extern Ptr(TagManager) tags; // temporarily

#endif
