#include "tagmanager.h"
#include <memory>

using namespace std;

Ptr(TagManager) tags;

TagManager::TagManager()
    : ChildByIndex()
    , by_name(*this)
{
}

