#include "rules.h"
#include "globals.h"
#include "utils.h"
#include "ewmh.h"
#include "client.h"
#include "ipc-protocol.h"
#include "hook.h"
#include "command.h"

#include "glib-backports.h"
#include "glib-backports.h"
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

/// DECLARATIONS ///
static int find_condition_type(const char* name);
static int find_consequence_type(const char* name);
static bool condition_string(HSCondition* rule, const char* string);

/// CONDITIONS ///
#define DECLARE_CONDITION(NAME)                         \
    static bool NAME(HSCondition* rule, HSClient* client)

DECLARE_CONDITION(condition_class);
DECLARE_CONDITION(condition_instance);
DECLARE_CONDITION(condition_title);
DECLARE_CONDITION(condition_pid);
DECLARE_CONDITION(condition_maxage);
DECLARE_CONDITION(condition_windowtype);
DECLARE_CONDITION(condition_windowrole);

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

static HSConditionType g_condition_types[] = {
    { "class",          condition_class             },
    { "instance",       condition_instance          },
    { "title",          condition_title             },
    { "pid",            condition_pid               },
    { "maxage",         condition_maxage            },
    { "windowtype",     condition_windowtype        },
    { "windowrole",     condition_windowrole        },
};

static int     g_maxage_type; // index of "maxage"
static time_t  g_current_rule_birth_time; // data from rules_apply() to condition_maxage()
static unsigned long long g_rule_label_index; // incremental index of rule label

static HSConsequenceType g_consequence_types[] = {
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

static std::list<HSRule *> g_rules;

/// FUNCTIONS ///
// RULES //
void rules_init() {
    g_maxage_type = find_condition_type("maxage");
    g_rule_label_index = 0;
}

void rules_destroy() {
    for (auto rule : g_rules) {
        delete rule;
    }
    g_rules.clear();
}

// condition types //
static int find_condition_type(const char* name) {
    const char* cn;
    for (int i = 0; i < LENGTH(g_condition_types); i++) {
        cn = g_condition_types[i].name;
        if (!cn) break;
        if (!strcmp(cn, name)) {
            return i;
        }
    }
    return -1;
}

bool HSRule::addCondition(int type, char op, char* value, Output output) {
    HSCondition cond;
    if (op != '=' && type == g_maxage_type) {
        output << "rule: Condition maxage only supports the = operator\n";
        return false;
    }
    switch (op) {
        case '=': {
            if (type == g_maxage_type) {
                cond.value_type = CONDITION_VALUE_TYPE_INTEGER;
                if (1 != sscanf(value, "%d", &cond.value.integer)) {
                    output << "rule: Can not integer from \"" << value << "\"\n";
                    return false;
                }
            } else {
                cond.value_type = CONDITION_VALUE_TYPE_STRING;
                cond.value.str = g_strdup(value);
            }
            break;
        }

        case '~': {
            cond.value_type = CONDITION_VALUE_TYPE_REGEX;
            int status = regcomp(&cond.value.reg.exp, value, REG_EXTENDED);
            if (status != 0) {
                char buf[ERROR_STRING_BUF_SIZE];
                regerror(status, &cond.value.reg.exp, buf, ERROR_STRING_BUF_SIZE);
                output << "rule: Can not parse value \"" << value
                        << "\" from condition \"" << g_condition_types[type].name
                        << "\": \"" << buf << "\"\n";
                return false;
            }
            cond.value.reg.str = g_strdup(value);
            break;
        }

        default:
            output << "rule: Unknown rule condition operation \"" << op << "\"\n";
            return false;
            break;
    }

    cond.condition_type = type;

    conditions.push_back(cond);
    return true;
}

static void condition_destroy(HSCondition* cond) {
    if (!cond) {
        return;
    }
    // free members
    switch(cond->value_type) {
        case CONDITION_VALUE_TYPE_STRING:
            free(cond->value.str);
            break;
        case CONDITION_VALUE_TYPE_REGEX:
            regfree(&cond->value.reg.exp);
            g_free(cond->value.reg.str);
            break;
        default:
            break;
    }
}

// consequence types //
static int find_consequence_type(const char* name) {
    const char* cn;
    for (int i = 0; i < LENGTH(g_consequence_types); i++) {
        cn = g_consequence_types[i].name;
        if (!cn) break;
        if (!strcmp(cn, name)) {
            return i;
        }
    }
    return -1;
}

/**
 * Add consequence to this rule.
 *
 * @retval false if the consequence cannot be added (malformed)
 */
bool HSRule::addConsequence(int type, char op, char* value, Output output) {
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

    cons.type = type;

    consequences.push_back(cons);
    return true;
}

bool HSRule::replaceLabel(char op, char* value, Output output) {
    switch (op) {
        case '=':
            if (*value == '\0') {
                output << "rule: Rule label cannot be empty";
                return false;
                break;
            }
            label = value;
            break;
        default:
            output << "rule: Unknown rule label operation \"" << op << "\"\n";
            return false;
            break;
    }
    return true;
}

// rules parsing //

HSRule::HSRule() {
    birth_time = get_monotonic_timestamp();
    label = std::to_string(g_rule_label_index++); // label defaults to index number
}

HSRule::~HSRule() {
    // free conditions
    for (auto& cond : conditions) {
        condition_destroy(&cond);
    }
}

void rule_complete(int argc, char** argv, int pos, Output output) {
    const char* needle = (pos < argc) ? argv[pos] : "";
    GString* buf = g_string_sized_new(20);

    // complete against conditions
    for (int i = 0; i < LENGTH(g_condition_types); i++) {
        g_string_printf(buf, "%s=", g_condition_types[i].name);
        try_complete_partial(needle, buf->str, output);
        g_string_printf(buf, "%s~", g_condition_types[i].name);
        try_complete_partial(needle, buf->str, output);
    }

    // complete against consequences
    for (int i = 0; i < LENGTH(g_consequence_types); i++) {
        g_string_printf(buf, "%s=", g_consequence_types[i].name);
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

// Looks up rules of a given label and removes them from the queue
static bool rule_find_pop(char* label) {
    bool status = false; // Will be returned as true if any is found

    // Note: This ugly loop can be replaced by a single std::erase statement
    // once g_rules is a container of unique pointers.
    auto ruleIter = g_rules.begin();
    while (ruleIter != g_rules.end()) {
        auto rule = *ruleIter;
        if (rule->label == label) {
            delete rule;
            status = true;
            ruleIter = g_rules.erase(ruleIter);
        } else {
            ruleIter++;
        }
    }

    return status;
}

// List all rules in queue
static void rule_print_append_output(HSRule* rule, std::ostream* ptr_output) {
    Output& output = *ptr_output;
    output << "label=" << rule->label << "\t";
    // Append conditions
    for (auto const& cond : rule->conditions) {
        if (cond.negated) { // Include flag if negated
            output << "not\t";
        }
        switch (cond.value_type) {
            case CONDITION_VALUE_TYPE_STRING:
                output << g_condition_types[cond.condition_type].name << "=";
                output << cond.value.str << "\t";
                break;
            case CONDITION_VALUE_TYPE_REGEX:
                output << g_condition_types[cond.condition_type].name << "~";
                output << cond.value.reg.str << "\t";
                break;
            default: /* CONDITION_VALUE_TYPE_INTEGER: */
                output << g_condition_types[cond.condition_type].name << "=";
                output << cond.value.integer << "\t";
                break;
        }
    }
    // Append consequences
    for (auto const& cons : rule->consequences) {
        output << g_consequence_types[cons.type].name
               << "=" << cons.value << "\t";
    }
    // Print new line
    output << '\n';
}

int rule_print_all_command(int argc, char** argv, Output output) {
    // Print entry for each in the queue
    for (auto rule : g_rules) {
        rule_print_append_output(rule, &output);
    }
    return 0;
}

// parses an arg like NAME=VALUE to res_name, res_operation and res_value
bool tokenize_arg(char* condition,
                  char** res_name, char* res_operation, char** res_value) {
    // ignore two leading dashes
    if (condition[0] == '-' && condition[1] == '-') {
        condition += 2;
    }

    // get name
    *res_name = condition;


    // locate operation
    char* op = strpbrk(condition, "=~");
    if (!op) {
        return false;
    }
    *res_operation = *op;
    *op = '\0'; // separate string at operation char

    // value is second one (starting after op character)
    *res_value = op + 1;
    return true;
}

int rule_add_command(int argc, char** argv, Output output) {
    // usage: rule COND=VAL ... then

    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    // temporary data structures
    HSRule* rule = new HSRule();
    bool printlabel = false;
    bool negated = false;
    bool prepend = false;
    struct {
        const char* name;
        bool* flag;
    } flags[] = {
        { "prepend",&prepend },
        { "not",    &negated },
        { "!",      &negated },
        { "once",   &rule->once },
        { "printlabel",&printlabel },
    };

    // parse rule incrementally. always maintain a correct rule in rule
    while (SHIFT(argc, argv)) {
        char* name;
        char* value;
        char op;

        // is it a consequence or a condition?
        bool consorcond = tokenize_arg(*argv, &name, &op, &value);
        int type;
        bool flag_found = false;
        int flag_index = -1;

        for (int i = 0; i < LENGTH(flags); i++) {
            if (!strcmp(flags[i].name, name)) {
                flag_found = true;
                flag_index = i;
                break;
            }
        }

        if (flag_found) {
            *flags[flag_index].flag = ! *flags[flag_index].flag;
        }

        else if (consorcond && (type = find_condition_type(name)) >= 0) {
            auto success = rule->addCondition(type, op, value, output);
            if (!success) {
                delete rule;
                return HERBST_INVALID_ARGUMENT;
            }
            rule->conditions.back().negated = negated;
            negated = false;
        }

        else if (consorcond && (type = find_consequence_type(name)) >= 0) {
            bool success = rule->addConsequence(type, op, value, output);
            if (!success) {
                delete rule;
                return HERBST_INVALID_ARGUMENT;
            }
        }

        // Check for a provided label, and replace default index if so
        else if (consorcond && (!strcmp(name,"label"))) {
            if (!rule->replaceLabel(op, value, output)) {
                delete rule;
                return HERBST_INVALID_ARGUMENT;
            }
        }

        else {
            // need to hardcode "rule:" here because args are shifted
            output << "rule: Unknown argument \"" << *argv << "\"\n";
            delete rule;
            return HERBST_INVALID_ARGUMENT;
        }
    }

    if (printlabel) {
       output << rule->label << "\n";
    }

    if (prepend) g_rules.push_front(rule);
    else         g_rules.push_back(rule);
    return 0;
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


int rule_remove_command(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }

    if (!strcmp(argv[1], "--all") || !strcmp(argv[1], "-F")) {
        // remove all rules
        rules_destroy();
        g_rule_label_index = 0;
        return 0;
    }

    // Deletes rule with given label
    if (!rule_find_pop(argv[1])) {
        output << "Couldn't find rule: \"" << argv[1] << "\"";
        return HERBST_INVALID_ARGUMENT;
    }
    return 0;
}

// rules applying //
void client_changes_init(HSClientChanges* changes, HSClient* client) {
    memset(changes, 0, sizeof(HSClientChanges));
    changes->tree_index = g_string_new("");
    changes->focus = false;
    changes->switchtag = false;
    changes->manage = true;
    changes->fullscreen = ewmh_is_fullscreen_set(client->window_);
    changes->keymask = g_string_new("");
}

void client_changes_free_members(HSClientChanges* changes) {
    if (!changes) return;
    if (changes->tag_name) {
        g_string_free(changes->tag_name, true);
    }
    if (changes->tree_index) {
        g_string_free(changes->tree_index, true);
    }
    if (changes->monitor_name) {
        g_string_free(changes->monitor_name, true);
    }
}

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
            int type = cond.condition_type;

            if (!rule_match && type != g_maxage_type) {
                // implement lazy AND &&
                // ... except for maxage
                continue;
            }

            matches = g_condition_types[type].
                matches(&cond, client);

            if (!matches && !cond.negated
                && cond.condition_type == g_maxage_type) {
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
            for (auto & cons : rule->consequences) {
                g_consequence_types[cons.type].
                    apply(&cons, client, changes);
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
static bool condition_string(HSCondition* rule, const char* string) {
    if (!rule || !string) {
        return false;
    }

    int status;
    regmatch_t match;
    int int_value;
    switch (rule->value_type) {
        case CONDITION_VALUE_TYPE_STRING:
            return !strcmp(string, rule->value.str);
            break;
        case CONDITION_VALUE_TYPE_REGEX:
            status = regexec(&rule->value.reg.exp, string, 1, &match, 0);
            // only accept it, if it matches the entire string
            if (status == 0
                && match.rm_so == 0
                && match.rm_eo == strlen(string)) {
                return true;
            } else {
                return false;
            }
            break;
        case CONDITION_VALUE_TYPE_INTEGER:
            return (1 == sscanf(string, "%d", &int_value)
                && int_value == rule->value.integer);
            break;
    }
    return false;
}

static bool condition_class(HSCondition* rule, HSClient* client) {
    GString* window_class = window_class_to_g_string(g_display, client->window_);
    bool match = condition_string(rule, window_class->str);
    g_string_free(window_class, true);
    return match;
}

static bool condition_instance(HSCondition* rule, HSClient* client) {
    GString* inst = window_instance_to_g_string(g_display, client->window_);
    bool match = condition_string(rule, inst->str);
    g_string_free(inst, true);
    return match;
}

static bool condition_title(HSCondition* rule, HSClient* client) {
    return condition_string(rule, client->title_().c_str());
}

static bool condition_pid(HSCondition* rule, HSClient* client) {
    if (client->pid_ < 0) {
        return false;
    }
    if (rule->value_type == CONDITION_VALUE_TYPE_INTEGER) {
        return rule->value.integer == client->pid_;
    } else {
        char buf[1000]; // 1kb ought to be enough for every int
        sprintf(buf, "%d", client->pid_);
        return condition_string(rule, buf);
    }
}

static bool condition_maxage(HSCondition* rule, HSClient* client) {
    time_t diff = get_monotonic_timestamp() - g_current_rule_birth_time;
    return (rule->value.integer >= diff);
}

static bool condition_windowtype(HSCondition* rule, HSClient* client) {
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
            return condition_string(rule, g_netatom_names[i]);
        }
    }

    // if no valid window type has been found,
    // it can not match
    return false;
}

static bool condition_windowrole(HSCondition* rule, HSClient* client) {
    GString* role = window_property_to_g_string(g_display, client->window_,
        ATOM("WM_WINDOW_ROLE"));
    if (!role) return false;
    bool match = condition_string(rule, role->str);
    g_string_free(role, true);
    return match;
}

/// CONSEQUENCES ///
void consequence_tag(HSConsequence* cons,
                     HSClient* client, HSClientChanges* changes) {
    if (changes->tag_name) {
        g_string_assign(changes->tag_name, cons->value.c_str());
    } else {
        changes->tag_name = g_string_new(cons->value.c_str());
    }
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
    g_string_assign(changes->tree_index, cons->value.c_str());
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
    if (changes->keymask) {
        g_string_assign(changes->keymask, cons->value.c_str());
    } else {
        changes->keymask = g_string_new(cons->value.c_str());
    }
}

void consequence_monitor(HSConsequence* cons, HSClient* client,
                            HSClientChanges* changes) {
    if (changes->monitor_name) {
        g_string_assign(changes->monitor_name, cons->value.c_str());
    } else {
        changes->monitor_name = g_string_new(cons->value.c_str());
    }
}
