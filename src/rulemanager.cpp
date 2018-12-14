#include "rulemanager.h"

#include "ipc-protocol.h"

/*!
 * Implements the "rule" IPC command
 */
int RuleManager::addRuleCommand(Input input, Output output) {
    // not implemented yet (add more tests first)
    return -1;
}

/*!
 * Implements the "unrule" IPC command
 */
int RuleManager::unruleCommand(Input input, Output output) {
    std::string arg;
    if (!(input >> arg))
        return HERBST_NEED_MORE_ARGS;

    if (arg == "--all" || arg == "-F") {
        // Remove all rules
        for (auto rule : g_rules) {
            delete rule;
        }
        g_rules.clear();
        g_rule_label_index = 0;
    } else {
        // Remove rule specified by argument
        auto removedCount = removeRule(arg);
        if (removedCount == 0) {
            output << "Couldn't find any rules with label \"" << arg << "\"";
            return HERBST_INVALID_ARGUMENT;
        }
    }

    return HERBST_EXIT_SUCCESS;
}

/*!
 * Implements the "list_rules" IPC command
 */
int RuleManager::listRulesCommand(Input input, Output output) {
    for (auto rule : g_rules) {
        rule->print(output);
    }

    return HERBST_EXIT_SUCCESS;
}

/*!
 * Removes all rules with the given label
 *
 * \returns number of removed rules
 */
size_t RuleManager::removeRule(std::string label) {
    size_t removedCount = 0;

    // Note: This ugly loop can be replaced by a single std::erase statement
    // once g_rules is a container of unique pointers.
    auto ruleIter = g_rules.begin();
    while (ruleIter != g_rules.end()) {
        auto rule = *ruleIter;
        if (rule->label == label) {
            delete rule;
            removedCount++;
            ruleIter = g_rules.erase(ruleIter);
        } else {
            ruleIter++;
        }
    }

    return removedCount;
}
