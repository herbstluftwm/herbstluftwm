#ifndef __HS_RULES_H_
#define __HS_RULES_H_

#include <regex.h>
#include <functional>

#include "types.h"

class HSClient;

enum {
    CONDITION_VALUE_TYPE_STRING,
    CONDITION_VALUE_TYPE_REGEX,
    CONDITION_VALUE_TYPE_INTEGER,
};

enum {
    CONSEQUENCE_VALUE_TYPE_STRING,
};

class HSCondition {
public:
    static const std::map<std::string, std::function<bool(HSCondition *, HSClient*)>> matchers;

    // int condition_type;
    std::string name;
    int value_type;
    bool negated;

    std::string value_str;
    int value_integer;
    regex_t value_reg_exp;
    std::string value_reg_str;
};

class HSClientChanges {
public:
    HSClientChanges(HSClient *client);

    // For tag_name and monitor_name, an empty string means "no change",
    // because empty strings are not considered valid here. TODO: Use
    // std::optional for this.
    std::string     tag_name;
    std::string     monitor_name;

    std::string     tree_index;
    bool            focus = false; // if client should get focus
    bool            switchtag = false; // if the tag may be switched for focusing it
    bool            manage = true; // whether we should manage it
    bool            fullscreen;
    std::string     keymask; // Which keymask rule should be applied for this client
};

class HSConsequence {
public:
    static const std::map<std::string, std::function<void(HSConsequence*, HSClient*, HSClientChanges*)>> appliers;

    // int     type;
    std::string name;
    int value_type;
    std::string value;
};

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
    bool addCondition(std::string name, char op, const char* value, bool negated, Output output);
    bool addConsequence(std::string name, char op, const char* value, Output output);

    void print(Output output);
};


void rules_init();
void rules_destroy();
void rules_apply(HSClient* client, HSClientChanges* changes);

void rule_complete(int argc, char** argv, int pos, Output output);

void complete_against_rule_names(int argc, char** argv, int pos, Output output);

#endif
