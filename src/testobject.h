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

    static std::shared_ptr<TestObject> tester() {
        auto object = std::make_shared<TestObject>();
        object->init(object);
        return object;
    }

private:
    Attribute_<int> foo_;
    Attribute_<bool> bar_;
    DynamicAttribute checker_;
    Action killer_;
};

}

#endif // TESTOBJECT_H
