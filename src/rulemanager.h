#pragma once

#include <list>
#include <memory>

#include "object.h"
#include "rules.h"


class RuleManager : public Object {
public:
    int addRuleCommand(Input input, Output output);
    void addRuleCompletion(Completion& complete);
    int unruleCommand(Input input, Output output);
    void unruleCompletion(Completion& complete);
    int listRulesCommand(Output output);
    HSClientChanges evaluateRules(HSClient* client);

private:
    size_t removeRule(std::string label);
    std::tuple<std::string, char, std::string> tokenize_arg(std::string arg);

    //! Ever-incrementing index for labeling new rules
    unsigned long long rule_label_index_ = 0;

    //! Currently active rules
    std::list<std::unique_ptr<HSRule>> rules_;
};
