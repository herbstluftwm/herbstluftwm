/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HS_RULES_H_
#define __HS_RULES_H_

#include <stdbool.h>
#include <regex.h>
#include <glib.h>

struct HSClient;
struct HSTag;

enum {
    CONDITION_VALUE_TYPE_STRING,
    CONDITION_VALUE_TYPE_REGEX,
};

enum {
    CONSEQUENCE_VALUE_TYPE_STRING,
};

typedef struct {
    int condition_type;
    int value_type;
    union {
        char*       str;
        regex_t     exp;
    } value;
} HSCondition;

typedef struct {
    int     type;
    int value_type;
    union {
        char*       str;
    } value;
} HSConsequence;

typedef struct {
    HSCondition**   conditions;
    int             condition_count;
    HSConsequence** consequences;
    int             consequence_count;
} HSRule;

typedef struct {
    GString*        tag_name;
} HSClientChanges;

void rules_init();
void rules_destroy();
void rules_apply(struct HSClient* client, HSClientChanges* changes);

void client_changes_init(HSClientChanges* changes);
void client_changes_free_members(HSClientChanges* changes);

HSRule* rule_create();
void rule_destroy(HSRule* rule);

int rule_add_command(int argc, char** argv);


#endif

