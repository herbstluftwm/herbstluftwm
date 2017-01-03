#include "tagmanager.h"
#include <memory>

using namespace std;

TagManager::TagManager()
    : ChildByIndex()
    , by_name(*this)
{
}

