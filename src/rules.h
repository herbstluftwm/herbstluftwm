/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HS_RULES_H_
#define __HS_RULES_H_

#include <stdbool.h>
#include <regex.h>
#include "glib-backports.h"

struct HSClient;
struct HSTag;

enum {
    CONDITION_VALUE_TYPE_STRING,
    CONDITION_VALUE_TYPE_REGEX,
    CONDITION_VALUE_TYPE_INTEGER,
};

enum {
    CONSEQUENCE_VALUE_TYPE_STRING,
};

typedef struct {
    int condition_type;
    int value_type;
    bool negated;
    union {
        char*       str;
        struct {
            regex_t     exp;
            char*       str;
        } reg;
        int         integer;
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
    char*           label;
    HSCondition**   conditions;
    int             condition_count;
    HSConsequence** consequences;
    int             consequence_count;
    bool            once;
    time_t          birth_time; // timestamp of at creation
} HSRule;

typedef struct {
    GString*        tag_name;
    GString*        tree_index;
    bool            focus; // if client should get focus
    bool            switchtag; // if the tag may be switched for focusing it
    bool            manage; // whether we should manage it
    bool            fullscreen;
    bool            ewmhnotify; // whether to send ewmh-notifications
    GString*        keymask; // Which keymask rule should be applied for this client
} HSClientChanges;

void rules_init();
void rules_destroy();
void rules_apply(struct HSClient* client, HSClientChanges* changes);

void client_changes_init(HSClientChanges* changes, struct HSClient* client);
void client_changes_free_members(HSClientChanges* changes);

HSRule* rule_create();
void rule_destroy(HSRule* rule);

void rule_complete(int argc, char** argv, int pos, GString* output);

int rule_add_command(int argc, char** argv, GString* output);
int rule_remove_command(int argc, char** argv, GString* output);
int rule_print_all_command(int argc, char** argv, GString* output);
void complete_against_rule_names(int argc, char** argv, int pos, GString* output);

#endif
