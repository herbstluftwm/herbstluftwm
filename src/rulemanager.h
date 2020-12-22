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
    ClientChanges evaluateRules(Client* client, Output output, ClientChanges changes = {});
    static int parseRule(Input input, Output output, Rule& rule, bool& prepend);

private:
    size_t removeRules(std::string label);
    static std::tuple<std::string, char, std::string> tokenizeArg(std::string arg);

    //! Ever-incrementing index for labeling new rules
    unsigned long long rule_label_index_ = 0;

    //! Currently active rules
    std::list<std::unique_ptr<Rule>> rules_;
};
