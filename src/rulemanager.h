#pragma once

#include <cstddef>
#include <list>
#include <memory>
#include <string>
#include <tuple>

#include "object.h"
#include "rules.h"
#include "types.h"

class Client;
class Completion;

class RuleManager : public Object {
public:
    int addRuleCommand(Input input, Output output);
    void addRuleCompletion(Completion& complete);
    int unruleCommand(Input input, Output output);
    void unruleCompletion(Completion& complete);
    int listRulesCommand(Output output);
    ClientChanges evaluateRules(Client* client);

private:
    size_t removeRules(std::string label);
    std::tuple<std::string, char, std::string> tokenizeArg(std::string arg);

    //! Ever-incrementing index for labeling new rules
    unsigned long long rule_label_index_ = 0;

    //! Currently active rules
    std::list<std::unique_ptr<Rule>> rules_;
};
