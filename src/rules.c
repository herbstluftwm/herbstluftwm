/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "rules.h"
#include "globals.h"
#include "utils.h"
#include "clientlist.h"
#include "ipc-protocol.h"

#include <glib.h>
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
static void consequence_tag(HSConsequence* cons, HSClient* client,
                            HSClientChanges* changes);

/// GLOBALS ///

static HSConditionType g_condition_types[] = {
    {   "class",    condition_class },
};

static HSConsequenceType g_consequence_types[] = {
    {   "tag",      consequence_tag },
};

GQueue g_rules = G_QUEUE_INIT; // a list of HSRule* elements

/// FUNCTIONS ///
// RULES //
void rules_init() {
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
    switch (op) {
        case '=':
            cond.value_type = CONDITION_VALUE_TYPE_STRING;
            cond.value.str = g_strdup(value);
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
    return g_new0(HSRule, 1);
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
    SHIFT(argc, argv);

    // temporary data structures
    HSRule* rule = rule_create();
    HSCondition* cond;
    HSConsequence* cons;
    bool negated = false;
    struct {
        char* name;
        bool* flag;
    } flags[] = {
        { "not", &negated }
    };

    while (argc > 0) {
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

        SHIFT(argc, argv);
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
void client_changes_init(HSClientChanges* changes) {
    memset(changes, 0, sizeof(HSClientChanges));
}

void client_changes_free_members(HSClientChanges* changes) {
    if (!changes) return;
    if (changes->tag_name) {
        g_string_free(changes->tag_name, true);
    }
}

// apply all rules to a certain client an save changes
void rules_apply(HSClient* client, HSClientChanges* changes) {
    GList* cur = g_rules.head;
    while (cur) {
        HSRule* rule = cur->data;
        bool matches = true;

        // check all conditions
        for (int i = 0; matches && i < rule->condition_count; i++) {
            int type = rule->conditions[i]->condition_type;
            matches = g_condition_types[type].
                matches(rule->conditions[i], client);
            if (rule->conditions[i]->negated) {
                matches = ! matches;
            }
        }

        if (matches) {
            // apply all consequences
            for (int i = 0; i < rule->consequence_count; i++) {
                int type = rule->consequences[i]->type;
                g_consequence_types[type].
                    apply(rule->consequences[i], client, changes);
            }
        }

        // try next
        cur = cur->next;
    }
}

/// CONDITIONS ///
bool condition_string(HSCondition* rule, char* string) {
    if (!rule || !string) {
        return false;
    }

    int status;
    regmatch_t match;
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
    }
    return false;
}

bool condition_class(HSCondition* rule, HSClient* client) {
    GString* window_class = window_class_to_g_string(g_display, client->window);
    bool match = condition_string(rule, window_class->str);
    g_string_free(window_class, true);
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

