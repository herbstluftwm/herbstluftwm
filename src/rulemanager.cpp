#include "rulemanager.h"

// TODO: Move type maps to a better place?
extern HSConditionType g_condition_types[];
extern HSConsequenceType g_consequence_types[];

int RuleManager::listRules(Input input, Output output) {
    for (auto rule : g_rules) {
        rule->print(output);
    }

    return 0;
}
