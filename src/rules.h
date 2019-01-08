#ifndef __HS_RULES_H_
#define __HS_RULES_H_

#include <functional>
#include <regex>

#include "types.h"
#include "utils.h"

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

    using Matcher = std::function<bool(HSCondition*, const HSClient*)>;
    static const std::map<std::string, Matcher> matchers;

    std::string name;
    int value_type;
    bool negated;

    std::string value_str;
    int value_integer;
    std::regex value_reg_exp;
    std::string value_reg_str;

    /*! Timestamp of when this condition (i.e. rule) was created, which is
     * needed for the maxage matcher.
     *
     * (KISS: Storing this for all conditions saves us from needing an entire
     * class hierarchy of specialized conditions/matchers, or extending all
     * matcher signatures.)
     */
    time_t conditionCreationTime;

private:
    bool matchesClass(const HSClient* client);
    bool matchesInstance(const HSClient* client);
    bool matchesTitle(const HSClient* client);
    bool matchesPid(const HSClient* client);
    bool matchesMaxage(const HSClient* client);
    bool matchesWindowtype(const HSClient* client);
    bool matchesWindowrole(const HSClient* client);

    bool matches(const std::string& string);
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

    std::string name;
    int value_type;
    std::string value;

private:
    void applyTag(HSClient* client, HSClientChanges* changes);
    void applyIndex(HSClient* client, HSClientChanges* changes);
    void applyFocus(HSClient* client, HSClientChanges* changes);
    void applySwitchtag(HSClient* client, HSClientChanges* changes);
    void applyManage(HSClient* client, HSClientChanges* changes);
    void applyPseudotile(HSClient* client, HSClientChanges* changes);
    void applyFullscreen(HSClient* client, HSClientChanges* changes);
    void applyEwmhrequests(HSClient* client, HSClientChanges* changes);
    void applyEwmhnotify(HSClient* client, HSClientChanges* changes);
    void applyHook(HSClient* client, HSClientChanges* changes);
    void applyKeymask(HSClient* client, HSClientChanges* changes);
    void applyMonitor(HSClient* client, HSClientChanges* changes);
};

class HSRule {
public:
    HSRule();

    std::string label;
    std::vector<HSCondition> conditions;
    std::vector<HSConsequence> consequences;
    bool once = false;
    time_t birth_time; // timestamp of at creation

    bool setLabel(char op, std::string value, Output output);
    bool addCondition(std::string name, char op, const char* value, bool negated, Output output);
    bool addConsequence(std::string name, char op, const char* value, Output output);

    void print(Output output);
};

#endif
