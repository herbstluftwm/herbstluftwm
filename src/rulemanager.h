#pragma once

#include <list>

#include "object.h"
#include "rules.h"

// TODO: Turn this into a private member of RuleManager after the transition:
extern std::list<HSRule *> g_rules;

// TODO: Turn this into a private member of RuleManager after the transition:
extern unsigned long long g_rule_label_index; // incremental index of rule label

class RuleManager : public Object {
public:
    int unruleCommand(Input input, Output output);
    int listRulesCommand(Input input, Output output);

private:
    size_t removeRule(std::string label);
};
