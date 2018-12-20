#pragma once

#include <list>

#include "object.h"
#include "rules.h"

// TODO: Turn this into a private member of RuleManager after the transition:
extern std::list<HSRule *> g_rules;

class RuleManager : public Object {
public:
    int addRuleCommand(Input input, Output output);
    void addRuleCompletion(Completion& complete);
    int unruleCommand(Input input, Output output);
    void unruleCompletion(Completion& complete);
    int listRulesCommand(Input input, Output output);

private:
    size_t removeRule(std::string label);
    std::tuple<std::string, char, std::string> tokenize_arg(std::string arg);

    //! Ever-incrementing index for labeling new rules
    unsigned long long rule_label_index_ = 0;
};
