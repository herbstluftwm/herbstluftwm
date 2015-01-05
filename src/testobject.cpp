#include "testobject.h"
#include "hook.h"

namespace herbstluft {

void test_object_system()
{
    auto root = std::make_shared<Directory>("root");

    auto tester = std::make_shared<TestObject>();
    root->addChild(tester);

    auto hooks = {
        std::make_shared<herbstluft::Hook>("tester.precious.name"),
        std::make_shared<herbstluft::Hook>("tester.precious.bar"),
        std::make_shared<herbstluft::Hook>("tester.precious.sweets.name"),
        std::make_shared<herbstluft::Hook>("tester.precious.sweets.foo"),
        std::make_shared<herbstluft::Hook>("tester.precious"),
    };
    for (auto h : hooks) {
        h->hook_into(root);
    }
    root->print("");
    auto wat = std::dynamic_pointer_cast<herbstluft::TestObjectII>(
                   tester->children().begin()->second);
    wat->do_stuff();
    tester->do_stuff();
    root->print("");
}

TestObject::TestObject()
    : Object("tester"),
      foo_("foo", true, true, 42),
      bar_("bar", true, false, true),
      checker_("checker", Type::ATTRIBUTE_COLOR, true, false),
      killer_("killer")
{
    wireAttributes({ &foo_, &bar_, &checker_ });
    wireActions({ &killer_ });

    auto foo = std::make_shared<TestObjectII>("precious");
    children_.insert(std::make_pair(foo->name(), foo));
}

std::string TestObject::read(const std::string &attr) const
{
    if (attr == "checker") {
        return "blue"; // always blue!
    }
    return Object::read(attr);
}

void TestObject::do_stuff()
{
}

TestObjectII::TestObjectII(const std::string &name)
    : Object(name),
      foo_("foo", true, true, 42),
      bar_("bar", true, false, true),
      checker_("checker", Type::ATTRIBUTE_COLOR, true, false),
      killer_("killer")
{
    wireAttributes({ &foo_, &bar_, &checker_ });
    wireActions({ &killer_ });

    auto foo = std::make_shared<Object>("sweets");
    addChild(foo);
    auto fooII = std::make_shared<Object>("cake");
    addChild(fooII);
}

void TestObjectII::do_stuff()
{
    bar_ = false;

    removeChild("sweets");
    auto foo = std::make_shared<TestObjectII>("sweets");
    foo->write("foo", "23");
    addChild(foo);
    foo->write("foo", "24");
    removeChild("sweets");
    auto footy = std::make_shared<TestObjectII>("sweets");
    footy->write("foo", "100");
    addChild(footy);
}

}
