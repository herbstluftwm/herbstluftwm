#include "testobject.h"

namespace herbstluft {

TestObject::TestObject()
    : Object("tester"),
      foo_("foo", 42),
      bar_("bar", true, false, true),
      checker_("checker", Type::ATTRIBUTE_COLOR),
      killer_("killer")
{}

void TestObject::init(std::weak_ptr<Object> self)
{
    wireAttributes(self, { &foo_, &bar_, &checker_ });
    wireActions(self, { &killer_ });
}

}
