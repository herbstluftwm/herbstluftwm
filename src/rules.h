#ifndef __HS_RULES_H_
#define __HS_RULES_H_

#include <functional>
#include <regex>

#include "types.h"
#include "utils.h"
#include "optional.h"

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

    using Matcher = std::function<bool(const HSCondition*, const HSClient*)>;
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
    bool matchesClass(const HSClient* client) const;
    bool matchesInstance(const HSClient* client) const;
    bool matchesTitle(const HSClient* client) const;
    bool matchesPid(const HSClient* client) const;
    bool matchesMaxage(const HSClient* client) const;
    bool matchesWindowtype(const HSClient* client) const;
    bool matchesWindowrole(const HSClient* client) const;

    bool matches(const std::string& string) const;
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

    std::experimental::optional<int> pseudotile;
};

class HSConsequence {
public:
    // TODO: Make HSClient* parameter const (requires changes to applyEwmh… methods)
    using Applier = std::function<void(const HSConsequence*, HSClient*, HSClientChanges*)>;
    static const std::map<std::string, Applier> appliers;

    std::string name;
    int value_type;
    std::string value;

private:
    void applyTag(const HSClient* client, HSClientChanges* changes) const;
    void applyIndex(const HSClient* client, HSClientChanges* changes) const;
    void applyFocus(const HSClient* client, HSClientChanges* changes) const;
    void applySwitchtag(const HSClient* client, HSClientChanges* changes) const;
    void applyManage(const HSClient* client, HSClientChanges* changes) const;
    void applyPseudotile(const HSClient* client, HSClientChanges* changes) const;
    void applyFullscreen(const HSClient* client, HSClientChanges* changes) const;
    void applyEwmhrequests(HSClient* client, HSClientChanges* changes) const;
    void applyEwmhnotify(HSClient* client, HSClientChanges* changes) const;
    void applyHook(const HSClient* client, HSClientChanges* changes) const;
    void applyKeymask(const HSClient* client, HSClientChanges* changes) const;
    void applyMonitor(const HSClient* client, HSClientChanges* changes) const;
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
