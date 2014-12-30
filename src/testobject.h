#ifndef TESTOBJECT_H
#define TESTOBJECT_H

#include "object.h"
#include "attribute_.h"

namespace herbstluft {

class TestObject : public Object
{
public:
    TestObject();
    void init(std::weak_ptr<Object> self);

    std::string read(const std::string &attr) const;

    void do_stuff();

private:
    Attribute_<int> foo_;
    Attribute_<bool> bar_;
    DynamicAttribute checker_;
    Action killer_;
};

class TestObjectII : public Object
{
public:
    TestObjectII(const std::string &name);
    void init(std::weak_ptr<Object> self);

    void do_stuff();

private:
    Attribute_<int> foo_;
    Attribute_<bool> bar_;
    DynamicAttribute checker_;
    Action killer_;
};

void test_object_system();

}

#endif // TESTOBJECT_H
