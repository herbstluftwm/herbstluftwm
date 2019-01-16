#include "rulemanager.h"

#include <cstring>

#include "completion.h"
#include "ipc-protocol.h"
#include "utils.h"

/*!
 * Implements the "rule" IPC command
 */
int RuleManager::addRuleCommand(Input input, Output output) {
    Rule rule;

    // Assign default label (index will be incremented if adding the rule
    // actually succeeds)
    rule.label = std::to_string(rule_label_index_);

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
            ruleFlags[arg] = true;
            continue;
        }

        // Check if arg is a condition negator
        if (arg == "not" || arg == "!") {
            // Make sure there is another argument coming:
            if (argIter + 1 == input.end()) {
                output << "Expected another argument after \""<< arg << "\" flag\n";
                return HERBST_INVALID_ARGUMENT;
            }

            // Skip forward to next argument, but remember that it is negated:
            negated = true;
            arg = *(++argIter);
        }

        // Tokenize arg, expect something like foo=bar or foo~bar:
        char oper;
        std::string lhs, rhs;
        std::tie(lhs, oper, rhs) = tokenize_arg(arg);

        // Check if lhs is a condition name
        if (Condition::matchers.count(lhs)) {
            bool success = rule.addCondition(lhs, oper, rhs.c_str(), negated, output);
            if (!success) {
                return HERBST_INVALID_ARGUMENT;
            }
            continue;
        }

        // Check if lhs is a consequence name
        if (Consequence::appliers.count(lhs)) {
            if (oper == '~') {
                output << "rule: Operator ~ not valid for consequence \"" << lhs << "\"\n";
                return HERBST_INVALID_ARGUMENT;
            }

            bool success = rule.addConsequence(lhs, oper, rhs.c_str(), output);
            if (!success) {
                return HERBST_INVALID_ARGUMENT;
            }
            continue;
        }


        // Check if arg is a custom label for this rule
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

    // Store "once" flag in rule
    rule.once = ruleFlags["once"];

    // Comply with "printlabel" flag
    if (ruleFlags["printlabel"]) {
       output << rule.label << "\n";
    }

    // At this point, adding the rule will be successful, so increment the
    // label index (as it says in the docs):
    rule_label_index_++;

    // Insert rule into list according to "prepend" flag
    auto insertAt = ruleFlags["prepend"] ? rules_.begin() : rules_.end();
    rules_.insert(insertAt, make_unique<Rule>(rule));

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
        rules_.clear();
        rule_label_index_ = 0;
    } else {
        // Remove rule specified by argument
        auto removedCount = removeRules(arg);
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
int RuleManager::listRulesCommand(Output output) {
    for (auto& rule : rules_) {
        rule->print(output);
    }

    return HERBST_EXIT_SUCCESS;
}

/*!
 * Removes all rules with the given label
 *
 * \returns number of removed rules
 */
size_t RuleManager::removeRules(std::string label) {
    auto countBefore = rules_.size();

    for (auto ruleIter = rules_.begin(); ruleIter != rules_.end();) {
        if ((*ruleIter)->label == label) {
            ruleIter = rules_.erase(ruleIter);
        } else {
            ruleIter++;
        }
    }

    auto countAfter = rules_.size();

    return countAfter - countBefore;
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

void RuleManager::unruleCompletion(Completion& complete) {
    complete.full({ "-F", "--all" });
    for (auto& it : rules_) {
        complete.full(it->label);
    }
}

void RuleManager::addRuleCompletion(Completion& complete) {
    complete.full({ "not", "!", "prepend", "once", "printlabel" });
    complete.partial("label=");
    for (auto&& matcher : Condition::matchers) {
        auto condName = matcher.first;
        complete.partial(condName + "=");
        complete.partial(condName + "~");
    }
    for (auto&& applier : Consequence::appliers) {
        complete.partial(applier.first + "=");
    }
}


//! Evaluate rules against a given client
HSClientChanges RuleManager::evaluateRules(Client* client) {
    HSClientChanges changes(client);

    auto ruleIter = rules_.begin();
    while (ruleIter != rules_.end()) {
        auto& rule = *ruleIter;
        bool matches = true;    // if current condition matches
        bool rule_match = true; // if entire rule matches
        bool rule_expired = false;

        // check all conditions
        for (auto& cond : rule->conditions) {
            if (!rule_match && cond.name != "maxage") {
                // implement lazy AND &&
                // ... except for maxage
                continue;
            }

            matches = Condition::matchers.at(cond.name)(&cond, client);

            if (!matches && !cond.negated
                && cond.name == "maxage") {
                // if if not negated maxage does not match anymore
                // then it will never match again in the future
                rule_expired = true;
            }

            if (cond.negated) {
                matches = ! matches;
            }
            rule_match = rule_match && matches;
        }

        if (rule_match) {
            // apply all consequences
            for (auto& cons : rule->consequences) {
                Consequence::appliers.at(cons.name)(&cons, client, &changes);
            }
        }

        // remove it if not wanted or needed anymore
        if ((rule_match && rule->once) || rule_expired) {
            ruleIter = rules_.erase(ruleIter);
        } else {
            // try next
            ruleIter++;
        }
    }

    return changes;
}


