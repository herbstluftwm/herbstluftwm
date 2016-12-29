#include "testobject.h"
#include "hookmanager.h"
#include "root.h"
#include "utils.h"


void test_object_system()
{
    //auto root = Root::get();

    //auto tester = std::make_shared<TestObject>();
    //root->addChild(tester);

    //auto hooks = {
    //    "tester.precious.name",
    //    "tester.precious.bar",
    //    "tester.precious.sweets.name",
    //    "tester.precious.sweets.foo",
    //    "tester.precious",
    //    "lasmiranda"
    //};
    //for (auto h : hooks) {
    //    root->hooks()->add(h);
    //}
    //root->print("");
    //auto wat = std::dynamic_pointer_cast<TestObjectII>(
    //               tester->children().begin()->second);
    //wat->do_stuff();
    //tester->do_stuff();
    //root->print("");
}

TestObject::TestObject()
    : foo_("foo", ACCEPT_ALL, 42)
    , bar_("bar", ACCEPT_ALL, true)
      // checker_("checker", Type::ATTRIBUTE_COLOR, false, false),
{
    wireAttributes({ &foo_, &bar_, });
    wireActions({ &killer_ });

    auto foo = std::make_shared<TestObjectII>();
    //children_.insert(std::make_pair(foo->name(), foo));
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

TestObjectII::TestObjectII()
    : foorious_("foorious", ACCEPT_ALL, 42),
      bar_("bar", ACCEPT_ALL, true)
      // checker_("checker", Type::ATTRIBUTE_COLOR, true, false),
{
    wireAttributes({ &foorious_, &bar_  });
    wireActions({ &killer_ });

    auto foo = std::make_shared<Object>();
    addChild(foo, "sweets");
    auto fooII = std::make_shared<Object>();
    addChild(fooII, "cake");
}

void TestObjectII::do_stuff()
{
    bar_ = false;

    removeChild("sweets");
    auto foo = std::make_shared<TestObjectII>();
    foo->write("foo", "23");
    addChild(foo, "sweets");
    foo->write("foo", "24");
    removeChild("sweets");
    auto footy = std::make_shared<TestObjectII>();
    footy->write("foo", "100");
    addChild(footy, "sweets");
}

