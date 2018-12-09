#ifndef __HS_RULES_H_
#define __HS_RULES_H_

#include "glib-backports.h"
#include "types.h"

#include <regex.h>

class HSClient;

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
    std::string value;
} HSConsequence;

class HSRule {
public:
    HSRule();
    ~HSRule();
    std::string label;
    std::vector<HSCondition> conditions;
    std::vector<HSConsequence> consequences;
    bool once = false;
    time_t birth_time; // timestamp of at creation

    bool replaceLabel(char op, char* value, Output output);
    bool addConsequence(int type, char op, char* value, Output output);
    bool addCondition(int type, char op, char* value, Output output);
};

typedef struct {
    GString*        tag_name;
    GString*        monitor_name;
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
void rules_apply(HSClient* client, HSClientChanges* changes);

void client_changes_init(HSClientChanges* changes, HSClient* client);
void client_changes_free_members(HSClientChanges* changes);

void rule_complete(int argc, char** argv, int pos, Output output);

int rule_add_command(int argc, char** argv, Output output);
int rule_remove_command(int argc, char** argv, Output output);
int rule_print_all_command(int argc, char** argv, Output output);
void complete_against_rule_names(int argc, char** argv, int pos, Output output);

#endif
