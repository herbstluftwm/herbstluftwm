#include "rulemanager.h"

#include <cstring>

#include "ipc-protocol.h"


/*!
 * Implements the "rule" IPC command
 */
int RuleManager::addRuleCommand(Input input, Output output) {
    // TODO: Drop this shift() as soon as the Input class has transitioned to
    // providing only arguments and nothing else.
    input.shift();

    HSRule rule;

    std::map<std::string, bool> ruleFlags = {
        {"once", false},
        {"printlabel", false},
        {"prepend", false},
    };

    std::map<std::string, std::pair<char, std::string>> assignments;

    // for (auto& arg : input) {
    for (auto argIter = input.begin(); argIter != input.end(); argIter++) {
        auto arg = *argIter;
        bool negated = false;
        (void) negated;

        // Check if arg is a general rule flag
        if (ruleFlags.count(arg)) {
            output << "Setting rule flag: " << arg << "\n";
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
        output << "Tokenized " << arg << " --> " << lhs << ", " << oper << ", " << rhs << "\n";

        assignments[lhs] = std::make_pair(oper, rhs);

        if (HSCondition::matchers.count(lhs)) {
            output << "It's a condition\n";
            rule.addCondition(lhs, oper, rhs.c_str(), negated, output);
            continue;
        }

        if (HSConsequence::appliers.count(lhs)) {
            if (oper == '~') {
                output << "Operator ~ not valid for consequence " << lhs << "\n";
                return HERBST_INVALID_ARGUMENT;
            }
            output << "It's a consequence\n";
            rule.addConsequence(lhs, oper, rhs.c_str(), output);
            continue;
        }

        if (lhs == "label") {
            rule.label = rhs;
        }

        output << "rule: Unknown argument \"" << arg << "\"\n";
        // return HERBST_INVALID_ARGUMENT;
    }

    for (auto const& item : assignments) {
        output << "Assignment " << item.first << ": " << item.second.first << ", " << item.second.second << "\n";
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
    input >> arg;

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
