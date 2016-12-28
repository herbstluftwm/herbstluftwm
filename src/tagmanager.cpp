#include "tagmanager.h"
#include <memory>

using namespace std;

TagManager::TagManager() : ChildByIndex("tags") {
    by_name = make_shared<Object>("by-name");
    addChild(by_name);
}

