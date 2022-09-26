#include "rulemanager.h"

#include <algorithm>
#include <string>

#include "completion.h"
#include "globals.h"
#include "ipc-protocol.h"
#include "utils.h"

using std::string;
using std::to_string;
using std::endl;
using std::unique_ptr;

/**
 * @brief RuleManager::parseRule
 * @param input
 * @param output
 * @param the rule to modify
 * @param whether the 'prepend' flag was given in input
 * @return
 */
int RuleManager::parseRule(Input input, Output output, Rule& rule, bool& prepend)
{
    // Possible flags that apply to the rule as a whole:
    std::map<string, bool> ruleFlags = {
        {"once", false},
        {"printlabel", false},
        {"prepend", false},
    };

    for (auto argIter = input.begin(); argIter != input.end(); argIter++) {
        auto arg = *argIter;
        arg = stripDoubleHyphens(arg);

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
                output.perror() << "Expected another argument after \""<< arg << "\" flag\n";
                return HERBST_INVALID_ARGUMENT;
            }

            // Skip forward to next argument, but remember that it is negated:
            negated = true;
            arg = *(++argIter);
            arg = stripDoubleHyphens(arg);
        }

        if (arg == "fixedsize") {
            Condition cond;
            cond.name = arg;
            cond.value_type = CONDITION_VALUE_TYPE_NO_ARG;
            cond.match_ = &Condition::matchesFixedSize;
            cond.negated = negated;
            rule.conditions.push_back(cond);
            continue;
        }

        // Tokenize arg, expect something like foo=bar or foo~bar:
        char oper;
        string lhs, rhs;
        try {
            std::tie(lhs, oper, rhs) = tokenizeArg(arg);
        } catch (std::invalid_argument &error) {
            output.perror() << error.what() << endl;
            return HERBST_INVALID_ARGUMENT;
        }

        // Check if lhs is a condition name
        Condition::Matchers::const_iterator maybe_condition = Condition::matchers.find(lhs);
        if (maybe_condition != Condition::matchers.end()) {
            bool success = rule.addCondition(maybe_condition, oper, rhs.c_str(), negated, output);
            if (!success) {
                return HERBST_INVALID_ARGUMENT;
            }
            continue;
        }

        // Check if lhs is a consequence name
        Consequence::Appliers::const_iterator maybe_consequence = Consequence::appliers.find(lhs);
        if (maybe_consequence != Consequence::appliers.end()) {
            if (oper == '~') {
                output.perror() << "Operator ~ not valid for consequence \"" << lhs << "\"\n";
                return HERBST_INVALID_ARGUMENT;
            }

            bool success = rule.addConsequence(maybe_consequence, rhs.c_str(), output);
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

        output.perror() << "Unknown argument \"" << arg << "\"\n";
        return HERBST_INVALID_ARGUMENT;
    }

    // Store "once" flag in rule
    rule.once = ruleFlags["once"];

    // Comply with "printlabel" flag
    if (ruleFlags["printlabel"]) {
       output << rule.label << "\n";
    }
    prepend = ruleFlags["prepend"];
    return 0;
}

int RuleManager::addRuleCommand(Input input, Output output) {
    Rule rule;
    bool prepend = false;

    // Assign default label (index will be incremented if adding the rule
    // actually succeeds)
    rule.label = to_string(rule_label_index_);
    int status = parseRule(input, output, rule, prepend);
    if (status != 0) {
        return status;
    }

    // At this point, adding the rule will be successful, so increment the
    // label index (as it says in the docs):
    rule_label_index_++;

    // Insert rule into list according to "prepend" flag
    auto insertAt = prepend ? rules_.begin() : rules_.end();
    rules_.insert(insertAt, make_unique<Rule>(rule));

    return HERBST_EXIT_SUCCESS;
}

/*!
 * Implements the "unrule" IPC command
 */
int RuleManager::unruleCommand(Input input, Output output) {
    string arg;
    if (!(input >> arg)) {
        return HERBST_NEED_MORE_ARGS;
    }

    if (arg == "--all" || arg == "-F") {
        rules_.clear();
        rule_label_index_ = 0;
    } else {
        // Remove rule specified by argument
        auto removedCount = removeRules(arg);
        if (removedCount == 0) {
            output.perror() << "Couldn't find any rules with label \"" << arg << "\"";
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
size_t RuleManager::removeRules(string label) {
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

std::string RuleManager::stripDoubleHyphens(string arg) {
    if (arg.substr(0, 2) == "--") {
        arg.erase(0, 2);
    }
    return arg;
}

std::tuple<string, char, string> RuleManager::tokenizeArg(string arg) {
    auto operPos = arg.find_first_of("~=");
    if (operPos == string::npos) {
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
    complete.full({ "fixedsize", "not", "!", "prepend", "once", "printlabel" });
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
ClientChanges RuleManager::evaluateRules(Client* client, Output output, ClientChanges changes) {
    // go through all rules and remove those that expired.
    // Here, we use erase + remove_if because it uses a Forward Iterator
    // and so it is ensured that the rules are evaluated in the correct order.
    auto forEachRule = [&](unique_ptr<Rule>& rule) {
        rule->evaluate(client, changes, output);
        return rule->expired();
    };
    rules_.erase(std::remove_if(rules_.begin(), rules_.end(), forEachRule),
                 rules_.end());
    return changes;
}


