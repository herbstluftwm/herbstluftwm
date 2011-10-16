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
    bool    (*matches)(HSClient* client, HSCondition* condition);
} HSConditionType;

typedef struct {
    char*   name;
    bool    (*apply)(HSClient* client, HSConsequence* consequence);
} HSConsequenceType;


static int find_condition_type(char* name);
static int find_consequence_type(char* name);

/// GLOBALS ///
static HSConditionType g_condition_types[] = {
    {   "class",    NULL },
};

static HSConsequenceType g_consequence_types[] = {
    {   "tag",    NULL },
};

GQueue g_rules = G_QUEUE_INIT; // a list of HSRule* elements

/// FUNCTIONS ///
void rules_init() {
}

void rules_destroy() {
    g_queue_foreach(&g_rules, (GFunc)rule_destroy, NULL);
    g_queue_clear(&g_rules);
}

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


// parses an arg like NAME=VALUE to res_name, res_operation and res_value
bool tokenize_arg(char* condition,
                  char** res_name, char* res_operation, char** res_value) {
    // ignore two leading dashes
    if (condition[0] == '-' && condition[1] == '-') {
        condition += 2;
    }

    // locate operation
    char* op = strpbrk(condition, "=~");
    if (!op) {
        return false;
    }
    *res_operation = *op;
    *op = '\0'; // separate string at operation char

    // get name
    *res_name = condition;

    // value is second one (starting after op character)
    *res_value = op + 1;
    return true;
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
            int status = regcomp(&cond.value.exp, value, REG_NOSUB|REG_EXTENDED);
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

HSConsequence* consequence_create(int type, char op, char* value) {
    HSConsequence cons;
    switch (op) {
        case '=':
            cons.value = g_strdup(value);
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
    g_free(cons->value);
    g_free(cons);
}

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

    while (argc > 0) {
        char* name;
        char* value;
        char op;

        // is it a consequence or a condition?
        bool consorcond = tokenize_arg(*argv, &name, &op, &value);
        int type;

        if (consorcond && (type = find_condition_type(name)) >= 0) {
            cond = condition_create(type, op, value);
            if (!cond) {
                rule_destroy(rule);
                return HERBST_INVALID_ARGUMENT;
            }
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





