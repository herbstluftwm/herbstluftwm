#ifndef __HLWM_TAGMANAGER_H_
#define __HLWM_TAGMANAGER_H_

#include "tag.h"
#include "childbyindex.h"

namespace herbstluft {

class TagManager : public ChildByIndex<HSTag> {
public:
    TagManager();
private:
    Ptr(Object) by_name;
};

}

#endif
