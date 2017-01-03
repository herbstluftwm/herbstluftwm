
#include "byname.h"

ByName::ByName(Object* parent) {
    parent->addStaticChild(this, "by-name");
}
