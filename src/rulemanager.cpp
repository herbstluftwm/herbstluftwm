#include "rulemanager.h"

#include <cstring>
#include <iostream>

#include "ipc-protocol.h"


/*!
 * Implements the "rule" IPC command
 */
int RuleManager::addRuleCommand(Input input, Output output) {
    HSRule rule;

    // Possible flags that apply to the rule as a whole:
    std::map<std::string, bool> ruleFlags = {
        {"once", false},
        {"printlabel", false},
        {"prepend", false},
    };

    for (auto argIter = input.begin(); argIter != input.end(); argIter++) {
        auto arg = *argIter;

        // Whether this argument is negated (only applies to conditions)
        bool negated = false;

        // Check if arg is a flag for the whole rule
        if (ruleFlags.count(arg)) {
            // output << "Setting rule flag: " << arg << "\n";
            ruleFlags[arg] = true;
            continue;
        }

        if (arg == "not" || arg == "!") {
            // Make sure there is another argument coming:
            if (argIter + 1 == input.end()) {
                // TODO: Add test for this case!
                output << "Expected another argument after \""<< arg << "\" flag\n";
                return HERBST_NEED_MORE_ARGS;
            }

            negated = true;
            arg = *(++argIter);
            output << "Encountered 'not', looking at next token: " << arg << "\n";
        }

        // Expect arg to be of form foo=bar or foo~bar
        char oper;
        std::string lhs, rhs;
        std::tie(lhs, oper, rhs) = tokenize_arg(arg);
        std::cerr << "Tokenized " << arg << " --> " << lhs << ", " << oper << ", " << rhs << "\n";

        if (HSCondition::matchers.count(lhs)) {
            std::cerr << "It's a condition\n";
            bool success = rule.addCondition(lhs, oper, rhs.c_str(), negated, output);
            if (!success) {
                return HERBST_INVALID_ARGUMENT;
            }

            continue;
        }

        if (HSConsequence::appliers.count(lhs)) {
            if (oper == '~') {
                output << "rule: Operator ~ not valid for consequence \"" << lhs << "\"\n";
                return HERBST_INVALID_ARGUMENT;
            }
            std::cerr << "It's a consequence\n";
            bool success = rule.addConsequence(lhs, oper, rhs.c_str(), output);
            if (!success) {
                return HERBST_INVALID_ARGUMENT;
            }
            continue;
        }

        if (lhs == "label") {
            bool success = rule.setLabel(oper, rhs, output);
            if (!success) {
                return HERBST_INVALID_ARGUMENT;
            }
            continue;
        }

        output << "rule: Unknown argument \"" << arg << "\"\n";
        return HERBST_INVALID_ARGUMENT;
    }

    rule.once = ruleFlags["once"];

    if (ruleFlags["printlabel"]) {
       output << rule.label << "\n";
    }

    auto insertAt = ruleFlags["prepend"] ? g_rules.begin() : g_rules.end();
    g_rules.insert(insertAt, new HSRule(rule));

    return HERBST_EXIT_SUCCESS;
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

std::tuple<std::string, char, std::string> RuleManager::tokenize_arg(std::string arg) {
    if (arg.substr(0, 2) == "--") {
        arg.erase(0, 2);
    }

    auto operPos = arg.find_first_of("~=");
    if (operPos == std::string::npos) {
        throw std::invalid_argument("No operator in given arg: " + arg);
    }
    auto lhs = arg.substr(0, operPos);
    auto oper = arg[operPos];
    auto rhs = arg.substr(operPos + 1);

    return std::make_tuple(lhs, oper, rhs);
}
