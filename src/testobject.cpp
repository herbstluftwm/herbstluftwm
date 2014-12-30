#include "testobject.h"

namespace herbstluft {

void test_object_system()
{
    auto hook = std::make_shared<herbstluft::Hook>("tester.precious.name");
    auto hookII = std::make_shared<herbstluft::Hook>("tester.precious.bar");
    auto hookIII = std::make_shared<herbstluft::Hook>("tester.precious.sweets.name");
    auto tester = herbstluft::TestObject::tester();
    hook->init(hook, tester);
    hookII->init(hookII, tester);
    hookIII->init(hookIII, tester);
    tester->print("| ");
    auto wat = std::dynamic_pointer_cast<herbstluft::TestObjectII>(
                   tester->children().begin()->second);
    wat->do_stuff();
    tester->do_stuff();
}

TestObject::TestObject()
    : Object("tester"),
      foo_("foo", true, true, 42),
      bar_("bar", true, false, true),
      checker_("checker", Type::ATTRIBUTE_COLOR, true, false),
      killer_("killer")
{}

void TestObject::init(std::weak_ptr<Object> self)
{
    Object::init(self);

    wireAttributes({ &foo_, &bar_, &checker_ });
    wireActions({ &killer_ });

    auto foo = std::make_shared<TestObjectII>("precious");
    foo->init(foo);
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
{}

void TestObjectII::init(std::weak_ptr<Object> self)
{
    Object::init(self);

    wireAttributes({ &foo_, &bar_, &checker_ });
    wireActions({ &killer_ });

    auto foo = std::make_shared<Object>("sweets");
    foo->init(foo);
    children_.insert(std::make_pair(foo->name(), foo));
    auto fooII = std::make_shared<Object>("cake");
    fooII->init(fooII);
    children_.insert(std::make_pair(fooII->name(), fooII));
}

void TestObjectII::do_stuff()
{
    bar_ = false;

    children_.erase("sweets");
    notifyHooks();
}

}
