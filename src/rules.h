/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HS_RULES_H_
#define __HS_RULES_H_

#include <stdbool.h>
#include <regex.h>

enum {
    CONDITION_VALUE_TYPE_STRING,
    CONDITION_VALUE_TYPE_REGEX,
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
    char*   value;
} HSConsequence;

typedef struct {
    HSCondition**   conditions;
    int             condition_count;
    HSConsequence** consequences;
    int             consequence_count;
} HSRule;

void rules_init();
void rules_destroy();

HSRule* rule_create();
void rule_destroy(HSRule* rule);

int rule_add_command(int argc, char** argv);


#endif

