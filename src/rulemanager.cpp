#include "rulemanager.h"

#include "ipc-protocol.h"

int RuleManager::listRules(Input input, Output output) {
    for (auto rule : g_rules) {
        rule->print(output);
    }

    return HERBST_EXIT_SUCCESS;
}
