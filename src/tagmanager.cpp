#include "tagmanager.h"
#include <memory>

using namespace std;

TagManager::TagManager() : ChildByIndex() {
    by_name = make_shared<Object>();
    addChild(by_name, "by-name");
}

