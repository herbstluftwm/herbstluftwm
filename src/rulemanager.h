#pragma once

#include <list>

#include "object.h"
#include "rules.h"

// TODO: Turn this into a private member of RuleManager after the transition:
extern std::list<HSRule *> g_rules;

class RuleManager : public Object {
public:
    int listRules(Input input, Output output);
};
