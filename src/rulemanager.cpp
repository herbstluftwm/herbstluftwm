#include "rulemanager.h"

// TODO: Move type maps to a better place?
extern HSConditionType g_condition_types[];
extern HSConsequenceType g_consequence_types[];

int RuleManager::listRules(Input input, Output output) {
    for (auto rule : g_rules) {
        output << "label=" << rule->label << "\t";

        // Append conditions
        for (auto const& cond : rule->conditions) {
            if (cond.negated) {
                output << "not\t";
            }
            output << g_condition_types[cond.condition_type].name << "=";
            switch (cond.value_type) {
                case CONDITION_VALUE_TYPE_STRING:
                    output << cond.value_str << "\t";
                    break;
                case CONDITION_VALUE_TYPE_REGEX:
                    output << cond.value_reg_str << "\t";
                    break;
                default: /* CONDITION_VALUE_TYPE_INTEGER: */
                    output << cond.value_integer << "\t";
                    break;
            }
        }

        // Append consequences
        for (auto const& cons : rule->consequences) {
            output << g_consequence_types[cons.type].name
                << "=" << cons.value << "\t";
        }

        // Append separating or final newline
        output << '\n';
    }

    return 0;
}
