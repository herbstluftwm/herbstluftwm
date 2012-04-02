/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "rules.h"
#include "globals.h"
#include "utils.h"
#include "ewmh.h"
#include "clientlist.h"
#include "ipc-protocol.h"

#include <glib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>


/// TYPES ///

typedef struct {
    char*   name;
    bool    (*matches)(HSCondition* condition, HSClient* client);
} HSConditionType;

typedef struct {
    char*   name;
    void    (*apply)(HSConsequence* consequence, HSClient* client,
                     HSClientChanges* changes);
} HSConsequenceType;

/// DECLARATIONS ///

static int find_condition_type(char* name);
static int find_consequence_type(char* name);
static bool condition_string(HSCondition* rule, char* string);
static bool condition_class(HSCondition* rule, HSClient* client);
static bool condition_instance(HSCondition* rule, HSClient* client);
static bool condition_title(HSCondition* rule, HSClient* client);
static bool condition_pid(HSCondition* rule, HSClient* client);
static bool condition_maxage(HSCondition* rule, HSClient* client);
static bool condition_windowtype(HSCondition* rule, HSClient* client);
static bool condition_windowrole(HSCondition* rule, HSClient* client);
static void consequence_tag(HSConsequence* cons, HSClient* client,
                            HSClientChanges* changes);
static void consequence_focus(HSConsequence* cons, HSClient* client,
                              HSClientChanges* changes);
static void consequence_manage(HSConsequence* cons, HSClient* client,
                              HSClientChanges* changes);
static void consequence_index(HSConsequence* cons, HSClient* client,
                              HSClientChanges* changes);
static void consequence_pseudotile(HSConsequence* cons, HSClient* client,
                                   HSClientChanges* changes);
static void consequence_fullscreen(HSConsequence* cons, HSClient* client,
                                   HSClientChanges* changes);

/// GLOBALS ///

static HSConditionType g_condition_types[] = {
    {   "class",    condition_class },
    {   "instance", condition_instance },
    {   "title",    condition_title },
    {   "pid",      condition_pid },
    {   "maxage",   condition_maxage },
    {   "windowtype", condition_windowtype },
    {   "windowrole", condition_windowrole },
};

int     g_maxage_type; // index of "maxage"
time_t  g_current_rule_birth_time; // data from rules_apply() to condition_maxage()

static HSConsequenceType g_consequence_types[] = {
    {   "tag",          consequence_tag },
    {   "index",        consequence_index },
    {   "focus",        consequence_focus },
    {   "manage",       consequence_manage },
    {   "pseudotile",   consequence_pseudotile },
    {   "fullscreen",   consequence_fullscreen },
};

GQueue g_rules = G_QUEUE_INIT; // a list of HSRule* elements

/// FUNCTIONS ///
// RULES //
void rules_init() {
    g_maxage_type = find_condition_type("maxage");
}

void rules_destroy() {
    g_queue_foreach(&g_rules, (GFunc)rule_destroy, NULL);
    g_queue_clear(&g_rules);
}

// condition types //
int find_condition_type(char* name) {
    char* cn;
    for (int i = 0; i < LENGTH(g_condition_types); i++) {
        cn = g_condition_types[i].name;
        if (!cn) break;
        if (!strcmp(cn, name)) {
            return i;
        }
    }
    return -1;
}

HSCondition* condition_create(int type, char op, char* value) {
    HSCondition cond;
    if (op != '=' && type == g_maxage_type) {
        fprintf(stderr, "condition maxage only supports the = operator\n");
        return NULL;
    }
    switch (op) {
        case '=':
            if (type == g_maxage_type) {
                cond.value_type = CONDITION_VALUE_TYPE_INTEGER;
                if (1 != sscanf(value, "%d", &cond.value.integer)) {
                    fprintf(stderr, "cannot integer from \"%s\"\n", value);
                    return NULL;
                }
            } else {
                cond.value_type = CONDITION_VALUE_TYPE_STRING;
                cond.value.str = g_strdup(value);
            }
            break;

        case '~':
            cond.value_type = CONDITION_VALUE_TYPE_REGEX;
            int status = regcomp(&cond.value.exp, value, REG_EXTENDED);
            if (status != 0) {
                char buf[ERROR_STRING_BUF_SIZE];
                regerror(status, &cond.value.exp, buf, ERROR_STRING_BUF_SIZE);
                fprintf(stderr, "Cannot parse value \"%s\"", value);
                fprintf(stderr, "from condition \"%s\": ",
                        g_condition_types[type].name);
                fprintf(stderr, "\"%s\"\n", buf);
                return NULL;
            }
            break;

        default:
            fprintf(stderr, "unknown rule condition operation \"%c\"\n", op);
            return NULL;
            break;
    }

    cond.condition_type = type;
    // move to heap
    HSCondition* ptr = g_new(HSCondition, 1);
    *ptr = cond;
    return ptr;
}

void condition_destroy(HSCondition* cond) {
    if (!cond) {
        return;
    }
    // free members
    switch(cond->value_type) {
        case CONDITION_VALUE_TYPE_STRING:
            free(cond->value.str);
            break;
        case CONDITION_VALUE_TYPE_REGEX:
            regfree(&cond->value.exp);
            break;
        default:
            break;
    }

    // free cond itself
    g_free(cond);
}

// consequence types //
int find_consequence_type(char* name) {
    char* cn;
    for (int i = 0; i < LENGTH(g_consequence_types); i++) {
        cn = g_consequence_types[i].name;
        if (!cn) break;
        if (!strcmp(cn, name)) {
            return i;
        }
    }
    return -1;
}

HSConsequence* consequence_create(int type, char op, char* value) {
    HSConsequence cons;
    switch (op) {
        case '=':
            cons.value_type = CONSEQUENCE_VALUE_TYPE_STRING;
            cons.value.str = g_strdup(value);
            break;

        default:
            fprintf(stderr, "unknown rule consequence operation \"%c\"\n", op);
            return NULL;
            break;
    }

    cons.type = type;
    // move to heap
    HSConsequence* ptr = g_new(HSConsequence, 1);
    *ptr = cons;
    return ptr;
}

void consequence_destroy(HSConsequence* cons) {
    switch (cons->value_type) {
        case CONSEQUENCE_VALUE_TYPE_STRING:
            g_free(cons->value.str);
            break;
    }
    g_free(cons);
}

// rules parsing //

HSRule* rule_create() {
    HSRule* rule = g_new0(HSRule, 1);
    rule->once = false;
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    rule->birth_time = t.tv_sec;
    return rule;
}

void rule_destroy(HSRule* rule) {
    // free conditions
    for (int i = 0; i < rule->condition_count; i++) {
        condition_destroy(rule->conditions[i]);
    }
    g_free(rule->conditions);
    // free consequences
    for (int i = 0; i < rule->consequence_count; i++) {
        consequence_destroy(rule->consequences[i]);
    }
    g_free(rule->consequences);
    // free rule itself
    g_free(rule);
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

static void rule_add_condition(HSRule* rule, HSCondition* cond) {
    rule->conditions = g_renew(HSCondition*,
                               rule->conditions, rule->condition_count + 1);
    rule->conditions[rule->condition_count] = cond;
    rule->condition_count++;
}

static void rule_add_consequence(HSRule* rule, HSConsequence* cons) {
    rule->consequences = g_renew(HSConsequence*,
                               rule->consequences, rule->consequence_count + 1);
    rule->consequences[rule->consequence_count] = cons;
    rule->consequence_count++;
}


int rule_add_command(int argc, char** argv) {
    // usage: rule COND=VAL ... then

    // temporary data structures
    HSRule* rule = rule_create();
    HSCondition* cond;
    HSConsequence* cons;
    bool negated = false;
    struct {
        char* name;
        bool* flag;
    } flags[] = {
        { "not",    &negated },
        { "!",      &negated },
        { "once",   &rule->once },
    };

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
            cond = condition_create(type, op, value);
            if (!cond) {
                rule_destroy(rule);
                return HERBST_INVALID_ARGUMENT;
            }
            cond->negated = negated;
            negated = false;
            rule_add_condition(rule, cond);
        }

        else if (consorcond && (type = find_consequence_type(name)) >= 0) {
            cons = consequence_create(type, op, value);
            if (!cons) {
                rule_destroy(rule);
                return HERBST_INVALID_ARGUMENT;
            }
            rule_add_consequence(rule, cons);
        }

        else {
            fprintf(stderr, "rule: unknown argument \"%s\"\n", *argv);
            return HERBST_INVALID_ARGUMENT;
        }
    }

    g_queue_push_tail(&g_rules, rule);
    return 0;
}

int rule_remove_command(int argc, char** argv) {
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }

    if (!strcmp(argv[1], "--all") || !strcmp(argv[1], "-F")) {
        // remove all rules
        g_queue_foreach(&g_rules, (GFunc)rule_destroy, NULL);
        g_queue_clear(&g_rules);
        return 0;
    }

    return HERBST_INVALID_ARGUMENT;
}

// rules applying //
void client_changes_init(HSClientChanges* changes, HSClient* client) {
    memset(changes, 0, sizeof(HSClientChanges));
    changes->tree_index = g_string_new("");
    changes->focus = false;
    changes->manage = true;
    changes->fullscreen = ewmh_is_fullscreen_set(client->window);
}

void client_changes_free_members(HSClientChanges* changes) {
    if (!changes) return;
    if (changes->tag_name) {
        g_string_free(changes->tag_name, true);
    }
    if (changes->tree_index) {
        g_string_free(changes->tree_index, true);
    }
}

// apply all rules to a certain client an save changes
void rules_apply(HSClient* client, HSClientChanges* changes) {
    GList* cur = g_rules.head;
    while (cur) {
        HSRule* rule = cur->data;
        bool matches = true;    // if current condition matches
        bool rule_match = true; // if entire rule matches
        bool rule_expired = false;
        g_current_rule_birth_time = rule->birth_time;

        // check all conditions
        for (int i = 0; i < rule->condition_count; i++) {
            int type = rule->conditions[i]->condition_type;

            if (!rule_match && type != g_maxage_type) {
                // implement lazy AND &&
                // ... except for maxage
                continue;
            }

            matches = g_condition_types[type].
                matches(rule->conditions[i], client);

            if (!matches && !rule->conditions[i]->negated
                && rule->conditions[i]->condition_type == g_maxage_type) {
                // if if not negated maxage doesnot match anymore
                // then it will never match again in the future
                rule_expired = true;
            }

            if (rule->conditions[i]->negated) {
                matches = ! matches;
            }
            rule_match = rule_match && matches;
        }

        if (rule_match) {
            // apply all consequences
            for (int i = 0; i < rule->consequence_count; i++) {
                int type = rule->consequences[i]->type;
                g_consequence_types[type].
                    apply(rule->consequences[i], client, changes);
            }

        }

        // remove it if not wanted or needed anymore
        if ((rule_match && rule->once) || rule_expired) {
            GList* next = cur->next;
            rule_destroy(cur->data);
            g_queue_remove_element(&g_rules, cur);
            cur = next;
            continue;
        }

        // try next
        cur = cur ? cur->next : NULL;
    }
}

/// CONDITIONS ///
bool condition_string(HSCondition* rule, char* string) {
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
            status = regexec(&rule->value.exp, string, 1, &match, 0);
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

bool condition_class(HSCondition* rule, HSClient* client) {
    GString* window_class = window_class_to_g_string(g_display, client->window);
    bool match = condition_string(rule, window_class->str);
    g_string_free(window_class, true);
    return match;
}

bool condition_instance(HSCondition* rule, HSClient* client) {
    GString* inst = window_instance_to_g_string(g_display, client->window);
    bool match = condition_string(rule, inst->str);
    g_string_free(inst, true);
    return match;
}

bool condition_title(HSCondition* rule, HSClient* client) {
    return condition_string(rule, client->title->str);
}


bool condition_pid(HSCondition* rule, HSClient* client) {
    if (client->pid < 0) {
        return false;
    }
    if (rule->value_type == CONDITION_VALUE_TYPE_INTEGER) {
        return rule->value.integer == client->pid;
    } else {
        char buf[1000]; // 1kb ought to be enough for every int
        sprintf(buf, "%d", client->pid);
        return condition_string(rule, buf);
    }
}

bool condition_maxage(HSCondition* rule, HSClient* client) {
    struct timespec cur;
    clock_gettime(CLOCK_MONOTONIC, &cur);
    time_t diff = cur.tv_sec - g_current_rule_birth_time;
    return (rule->value.integer >= diff);
}

bool condition_windowtype(HSCondition* rule, HSClient* client) {
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
            client->window,
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
    if(status != Success || bytes_left > 0 || items < 1 || buf == NULL) {
        return false;
    } else {
        wintype= *(Atom *)buf;
        XFree(buf);
    }

    for (int i = NetWmWindowTypeFIRST; i <= NetWmWindowTypeLAST; i++) {
        // try to find the window type
        if (wintype == g_netatom[i]) {
            // if found, then treat the window type as a string value,
            // which is registred in g_netatom_names[]
            return condition_string(rule, g_netatom_names[i]);
        }
    }

    // if no valid window type has been found,
    // it can not match
    return false;
}

bool condition_windowrole(HSCondition* rule, HSClient* client) {
    GString* role = window_property_to_g_string(g_display, client->window,
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
        changes->tag_name = g_string_assign(changes->tag_name, cons->value.str);
    } else {
        changes->tag_name = g_string_new(cons->value.str);
    }
}

void consequence_focus(HSConsequence* cons, HSClient* client,
                       HSClientChanges* changes) {
    changes->focus = string_to_bool(cons->value.str, changes->focus);
}

void consequence_manage(HSConsequence* cons, HSClient* client,
                        HSClientChanges* changes) {
    changes->manage = string_to_bool(cons->value.str, changes->manage);
}

void consequence_index(HSConsequence* cons, HSClient* client,
                               HSClientChanges* changes) {
    changes->tree_index = g_string_assign(changes->tree_index, cons->value.str);
}

void consequence_pseudotile(HSConsequence* cons, HSClient* client,
                            HSClientChanges* changes) {
    client->pseudotile = string_to_bool(cons->value.str, client->pseudotile);
}

void consequence_fullscreen(HSConsequence* cons, HSClient* client,
                            HSClientChanges* changes) {
    changes->fullscreen = string_to_bool(cons->value.str, changes->fullscreen);
}

