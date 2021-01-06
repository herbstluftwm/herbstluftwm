#pragma once

#include <map>
#include <string>

#include "attribute_.h"
#include "object.h"
#include "types.h"

class Completion;

class Watchers : public Object {
public:
    Watchers();
    void injectDependencies(Object* root);
    void scanForChanges();

    DynAttribute_<unsigned long> count_;

    int watchCommand(Input input, Output output);
    void watchCompletion(Completion& complete);
private:
    unsigned long count() const { return lastValue_.size(); }
    Object* root_ = nullptr;
    std::map<std::string, std::string> lastValue_;
};
