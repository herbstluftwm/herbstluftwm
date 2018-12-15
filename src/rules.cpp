#include "rules.h"
#include "globals.h"
#include "utils.h"
#include "ewmh.h"
#include "client.h"
#include "ipc-protocol.h"
#include "hook.h"
#include "command.h"

#include <cstring>
#include <cstdio>
#include <sys/types.h>
#include <list>
#include <algorithm>

/// TYPES ///

typedef struct {
    const char*   name;
    bool    (*matches)(HSCondition* condition, HSClient* client);
} HSConditionType;

typedef struct {
    const char*   name;
    void    (*apply)(HSConsequence* consequence, HSClient* client,
                     HSClientChanges* changes);
} HSConsequenceType;

/// CONDITIONS ///

/// CONSEQUENCES ///
#define DECLARE_CONSEQUENCE(NAME)                           \
static void NAME(HSConsequence* cons, HSClient* client,     \
                     HSClientChanges* changes)

DECLARE_CONSEQUENCE(consequence_tag);
DECLARE_CONSEQUENCE(consequence_focus);
DECLARE_CONSEQUENCE(consequence_manage);
DECLARE_CONSEQUENCE(consequence_index);
DECLARE_CONSEQUENCE(consequence_pseudotile);
DECLARE_CONSEQUENCE(consequence_fullscreen);
DECLARE_CONSEQUENCE(consequence_switchtag);
DECLARE_CONSEQUENCE(consequence_ewmhrequests);
DECLARE_CONSEQUENCE(consequence_ewmhnotify);
DECLARE_CONSEQUENCE(consequence_hook);
DECLARE_CONSEQUENCE(consequence_keymask);
DECLARE_CONSEQUENCE(consequence_monitor);

/// GLOBALS ///

const std::map<std::string, std::function<bool(HSCondition * ,HSClient*)>> HSCondition::matchers = {
    { "class",          &HSCondition::matchesClass             },
    { "instance",       &HSCondition::matchesInstance          },
    { "title",          &HSCondition::matchesTitle             },
    { "pid",            &HSCondition::matchesPid               },
    { "maxage",         &HSCondition::matchesMaxage            },
    { "windowtype",     &HSCondition::matchesWindowtype        },
    { "windowrole",     &HSCondition::matchesWindowrole        },
};

static time_t  g_current_rule_birth_time; // data from rules_apply() to condition_maxage()
unsigned long long g_rule_label_index; // incremental index of rule label

const std::map<std::string, std::function<void(HSConsequence*, HSClient*, HSClientChanges*)>> HSConsequence::appliers = {
    { "tag",            consequence_tag             },
    { "index",          consequence_index           },
    { "focus",          consequence_focus           },
    { "switchtag",      consequence_switchtag       },
    { "manage",         consequence_manage          },
    { "pseudotile",     consequence_pseudotile      },
    { "fullscreen",     consequence_fullscreen      },
    { "ewmhrequests",   consequence_ewmhrequests    },
    { "ewmhnotify",     consequence_ewmhnotify      },
    { "hook",           consequence_hook            },
    { "keymask",        consequence_keymask         },
    { "monitor",        consequence_monitor         },
};

std::list<HSRule *> g_rules;

/// FUNCTIONS ///
// RULES //
void rules_init() {
    g_rule_label_index = 0;
}

void rules_destroy() {
    for (auto rule : g_rules) {
        delete rule;
    }
    g_rules.clear();
}

bool HSRule::addCondition(std::string name, char op, const char* value, bool negated, Output output) {
    HSCondition cond;
    cond.negated = negated;

    if (op != '=' && name == "maxage") {
        output << "rule: Condition maxage only supports the = operator\n";
        return false;
    }
    switch (op) {
        case '=': {
            if (name == "maxage") {
                cond.value_type = CONDITION_VALUE_TYPE_INTEGER;
                if (1 != sscanf(value, "%d", &cond.value_integer)) {
                    output << "rule: Can not integer from \"" << value << "\"\n";
                    return false;
                }
            } else {
                cond.value_type = CONDITION_VALUE_TYPE_STRING;
                cond.value_str = value;
            }
            break;
        }

        case '~': {
            cond.value_type = CONDITION_VALUE_TYPE_REGEX;
            try {
                cond.value_reg_exp = std::regex(value, std::regex::extended);
            } catch(std::regex_error& err) {
                output << "rule: Can not parse value \"" << value
                        << "\" from condition \"" << name
                        << "\": \"" << err.what() << "\"\n";
                return false;
            }
            cond.value_reg_str = value;
            break;
        }

        default:
            output << "rule: Unknown rule condition operation \"" << op << "\"\n";
            return false;
            break;
    }

    cond.name = name;

    conditions.push_back(cond);
    return true;
}

/**
 * Add consequence to this rule.
 *
 * @retval false if the consequence cannot be added (malformed)
 */
bool HSRule::addConsequence(std::string name, char op, const char* value, Output output) {
    HSConsequence cons;
    switch (op) {
        case '=':
            cons.value_type = CONSEQUENCE_VALUE_TYPE_STRING;
            cons.value = value;
            break;

        default:
            output << "rule: Unknown rule consequence operation \"" << op << "\"\n";
            return false;
            break;
    }

    cons.name = name;

    consequences.push_back(cons);
    return true;
}

bool HSRule::setLabel(char op, std::string value, Output output) {
    if (op != '=') {
        output << "rule: Unknown rule label operation \"" << op << "\"\n";
        return false;
    }

    if (value.empty()) {
        output << "rule: Rule label cannot be empty\n";
        return false;
    }

    label = value;
    return true;
}

// rules parsing //

HSRule::HSRule() {
    birth_time = get_monotonic_timestamp();
    label = std::to_string(g_rule_label_index++); // label defaults to index number
}

void rule_complete(int argc, char** argv, int pos, Output output) {
    const char* needle = (pos < argc) ? argv[pos] : "";
    GString* buf = g_string_sized_new(20);

    // complete against conditions
    for (auto&& matcher : HSCondition::matchers) {
        auto condName = matcher.first;
        g_string_printf(buf, "%s=", condName.c_str());
        try_complete_partial(needle, buf->str, output);
        g_string_printf(buf, "%s~", condName.c_str());
        try_complete_partial(needle, buf->str, output);
    }

    // complete against consequences
    for (auto&& applier : HSConsequence::appliers) {
        auto applierName = applier.first;
        g_string_printf(buf, "%s=", applierName.c_str());
        try_complete_partial(needle, buf->str, output);
    }

    // complete label
    try_complete_partial(needle, "label=", output);
    // complete flags
    try_complete(needle, "prepend", output);
    try_complete(needle, "once",    output);
    try_complete(needle, "not",     output);
    try_complete(needle, "!",       output);
    try_complete(needle, "printlabel", output);

    g_string_free(buf, true);
}

void HSRule::print(Output output) {
    output << "label=" << label << "\t";

    // Append conditions
    for (auto const& cond : conditions) {
        if (cond.negated) {
            output << "not\t";
        }
        output << cond.name;
        switch (cond.value_type) {
            case CONDITION_VALUE_TYPE_STRING:
                output << "=" << cond.value_str << "\t";
                break;
            case CONDITION_VALUE_TYPE_REGEX:
                output << "~" << cond.value_reg_str << "\t";
                break;
            default: /* CONDITION_VALUE_TYPE_INTEGER: */
                output << "=" << cond.value_integer << "\t";
        }
    }

    // Append consequences
    for (auto const& cons : consequences) {
        output << cons.name << "=" << cons.value << "\t";
    }

    // Append separating or final newline
    output << '\n';
}

void complete_against_rule_names(int argc, char** argv, int pos, Output output) {
    const char* needle;
    if (pos >= argc) {
        needle = "";
    } else {
        needle = argv[pos];
    }
    // Complete labels
    for (auto rule : g_rules) {
        try_complete(needle, rule->label.c_str(), output);
    }
}

// rules applying //
HSClientChanges::HSClientChanges(HSClient *client)
    : fullscreen(ewmh_is_fullscreen_set(client->window_))
{}

// apply all rules to a certain client an save changes
void rules_apply(HSClient* client, HSClientChanges* changes) {
    auto ruleIter = g_rules.begin();
    while (ruleIter != g_rules.end()) {
        auto rule = *ruleIter;
        bool matches = true;    // if current condition matches
        bool rule_match = true; // if entire rule matches
        bool rule_expired = false;
        g_current_rule_birth_time = rule->birth_time;

        // check all conditions
        for (auto& cond : rule->conditions) {
            if (!rule_match && cond.name != "maxage") {
                // implement lazy AND &&
                // ... except for maxage
                continue;
            }

            matches = HSCondition::matchers.at(cond.name)(&cond, client);

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
                HSConsequence::appliers.at(cons.name)(&cons, client, changes);
            }
        }

        // remove it if not wanted or needed anymore
        if ((rule_match && rule->once) || rule_expired) {
            delete rule;
            ruleIter = g_rules.erase(ruleIter);
        } else {
            // try next
            ruleIter++;
        }
    }
}

/// CONDITIONS ///
bool HSCondition::matches(const std::string& string) {
    switch (value_type) {
        case CONDITION_VALUE_TYPE_STRING:
            return value_str == string;
            break;
        case CONDITION_VALUE_TYPE_REGEX:
            return std::regex_match(string, value_reg_exp);
            break;
        case CONDITION_VALUE_TYPE_INTEGER:
            try {
                return std::stoi(string) == value_integer;
            } catch (std::exception&) {
                return false;
            }
            break;
    }
    return false;
}

bool HSCondition::matchesClass(HSClient* client) {
    GString* window_class = window_class_to_g_string(g_display, client->window_);
    bool match = matches(window_class->str);
    g_string_free(window_class, true);
    return match;
}

bool HSCondition::matchesInstance(HSClient* client) {
    GString* inst = window_instance_to_g_string(g_display, client->window_);
    bool match = matches(inst->str);
    g_string_free(inst, true);
    return match;
}

bool HSCondition::matchesTitle(HSClient* client) {
    return matches(client->title_());
}

bool HSCondition::matchesPid(HSClient* client) {
    if (client->pid_ < 0) {
        return false;
    }
    if (value_type == CONDITION_VALUE_TYPE_INTEGER) {
        return value_integer == client->pid_;
    } else {
        char buf[1000]; // 1kb ought to be enough for every int
        sprintf(buf, "%d", client->pid_);
        return matches(buf);
    }
}

bool HSCondition::matchesMaxage(HSClient* client) {
    time_t diff = get_monotonic_timestamp() - g_current_rule_birth_time;
    return (value_integer >= diff);
}

bool HSCondition::matchesWindowtype(HSClient* client) {
    // that only works for atom-type utf8-string, _NET_WM_WINDOW_TYPE is int
    //  GString* wintype=
    //      window_property_to_g_string(g_display, client->window, wintype_atom);
    // =>
    // kinda duplicate from src/utils.c:window_properties_to_g_string()
    // using different xatom type, and only calling XGetWindowProperty
    // once, because we are sure we only need four bytes
    long bufsize = 10;
    char *buf;
    Atom type_ret, wintype;
    int format;
    unsigned long items, bytes_left;
    long offset = 0;

    int status = XGetWindowProperty(
            g_display,
            client->window_,
            g_netatom[NetWmWindowType],
            offset,
            bufsize,
            False,
            ATOM("ATOM"),
            &type_ret,
            &format,
            &items,
            &bytes_left,
            (unsigned char**)&buf
            );
    // we only need precisely four bytes (one Atom)
    // if there are bytes left, something went wrong
    if(status != Success || bytes_left > 0 || items < 1 || buf == nullptr) {
        return false;
    } else {
        wintype= *(Atom *)buf;
        XFree(buf);
    }

    for (int i = NetWmWindowTypeFIRST; i <= NetWmWindowTypeLAST; i++) {
        // try to find the window type
        if (wintype == g_netatom[i]) {
            // if found, then treat the window type as a string value,
            // which is registered in g_netatom_names[]
            return matches(g_netatom_names[i]);
        }
    }

    // if no valid window type has been found,
    // it can not match
    return false;
}

bool HSCondition::matchesWindowrole(HSClient* client) {
    GString* role = window_property_to_g_string(g_display, client->window_,
        ATOM("WM_WINDOW_ROLE"));
    if (!role) return false;
    bool match = matches(role->str);
    g_string_free(role, true);
    return match;
}

/// CONSEQUENCES ///
void consequence_tag(HSConsequence* cons,
                     HSClient* client, HSClientChanges* changes) {
    changes->tag_name = cons->value;
}

void consequence_focus(HSConsequence* cons, HSClient* client,
                       HSClientChanges* changes) {
    changes->focus = string_to_bool(cons->value, changes->focus);
}

void consequence_manage(HSConsequence* cons, HSClient* client,
                        HSClientChanges* changes) {
    changes->manage = string_to_bool(cons->value, changes->manage);
}

void consequence_index(HSConsequence* cons, HSClient* client,
                               HSClientChanges* changes) {
    changes->tree_index = cons->value;
}

void consequence_pseudotile(HSConsequence* cons, HSClient* client,
                            HSClientChanges* changes) {
    client->pseudotile_ = string_to_bool(cons->value, client->pseudotile_);
}

void consequence_fullscreen(HSConsequence* cons, HSClient* client,
                            HSClientChanges* changes) {
    changes->fullscreen = string_to_bool(cons->value, changes->fullscreen);
}

void consequence_switchtag(HSConsequence* cons, HSClient* client,
                           HSClientChanges* changes) {
    changes->switchtag = string_to_bool(cons->value, changes->switchtag);
}

void consequence_ewmhrequests(HSConsequence* cons, HSClient* client,
                              HSClientChanges* changes) {
    // this is only a flag that is unused during initialization (during
    // manage()) and so can be directly changed in the client
    client->ewmhrequests_ = string_to_bool(cons->value, client->ewmhrequests_);
}

void consequence_ewmhnotify(HSConsequence* cons, HSClient* client,
                            HSClientChanges* changes) {
    client->ewmhnotify_ = string_to_bool(cons->value, client->ewmhnotify_);
}

void consequence_hook(HSConsequence* cons, HSClient* client,
                            HSClientChanges* changes) {
    GString* winid = g_string_sized_new(20);
    g_string_printf(winid, "0x%lx", client->window_);
    const char* hook_str[] = { "rule" , cons->value.c_str(), winid->str };
    hook_emit(LENGTH(hook_str), hook_str);
    g_string_free(winid, true);
}

void consequence_keymask(HSConsequence* cons,
                         HSClient* client, HSClientChanges* changes) {
    changes->keymask = cons->value;
}

void consequence_monitor(HSConsequence* cons, HSClient* client,
                            HSClientChanges* changes) {
    changes->monitor_name = cons->value;
}
