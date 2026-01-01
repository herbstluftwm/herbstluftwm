#ifndef __HS_RULES_H_
#define __HS_RULES_H_

#include <functional>
#include <regex>

#include "converter.h"
#include "finite.h"
#include "optional.h"
#include "regexstr.h"

class Client;

enum {
    CONDITION_VALUE_TYPE_STRING,
    CONDITION_VALUE_TYPE_REGEX,
    CONDITION_VALUE_TYPE_INTEGER,
    CONDITION_VALUE_TYPE_NO_ARG,
};

enum {
    CONSEQUENCE_VALUE_TYPE_STRING,
};

class Condition {
public:

    using Matcher = std::function<bool(const Condition*, const Client*)>;
    using Matchers = const std::map<std::string, Matcher>;
    static Matchers matchers;

    std::string name;
    int value_type = 0;
    bool negated = false;

    std::string value_str;
    int value_integer = 0;
    std::regex value_reg_exp;
    std::string value_reg_str;
    Matcher match_;

    /*! Timestamp of when this condition (i.e. rule) was created, which is
     * needed for the maxage matcher.
     *
     * (KISS: Storing this for all conditions saves us from needing an entire
     * class hierarchy of specialized conditions/matchers, or extending all
     * matcher signatures.)
     */
    time_t conditionCreationTime = 0;

    bool matchesFixedSize(const Client* client) const;
private:
    bool matchesClass(const Client* client) const;
    bool matchesInstance(const Client* client) const;
    bool matchesTitle(const Client* client) const;
    bool matchesPid(const Client* client) const;
    bool matchesPgid(const Client* client) const;
    bool matchesMaxage(const Client* client) const;
    bool matchesWindowtype(const Client* client) const;
    bool matchesWindowrole(const Client* client) const;

    bool matches(const std::string& string) const;
};

/**
 * @brief The ClientPlacement enum configures
 * how a client is placed initially, that is, how the
 * coordinates in the floating rectangle of the client
 * are modified.
 */
enum class ClientPlacement {
    Center, //! place in the center of a monitor
    Unchanged, //! don't change the position
    Smart, //! as little overlaps as possible
};

template <>
struct is_finite<ClientPlacement> : std::true_type {};
template<> Finite<ClientPlacement>::ValueList Finite<ClientPlacement>::values;

class ClientChanges {
public:
    ClientChanges();

    // For tag_name and monitor_name, an empty string means "no change",
    // because empty strings are not considered valid here. TODO: Use
    // std::optional for this.
    std::string     tag_name;
    std::string     monitor_name;

    std::string     tree_index;
    bool            focus = false; // if client should get focus
    bool            switchtag = false; // if the tag may be switched for focusing it
    bool            manage = true; // whether we should manage it
    ClientPlacement floatplacement = ClientPlacement::Unchanged;
    std::experimental::optional<bool> fullscreen;
    std::experimental::optional<bool> sticky;
    std::experimental::optional<RegexStr> keyMask; // Which keymask rule should be applied for this client
    std::experimental::optional<RegexStr> keysInactive; // Which keymask rule should be applied for this client

    std::experimental::optional<bool> decorated;
    std::experimental::optional<bool> floating;
    std::experimental::optional<Rectangle> floatingGeometry;
    std::experimental::optional<bool> pseudotile;
    std::experimental::optional<bool> ewmhRequests;
    std::experimental::optional<bool> ewmhNotify;
};

class Consequence {
public:
    /*! An Applier modifies the given ClientChanges object, and possibly throws an
     * exception (std::invalid_argument, std::out_of_range) on run-time errors.
     */
    using Applier = std::function<void(const Consequence*, const Client*, ClientChanges*)>;
    /*! An entry in the Appliers map returns an Applier, given the argument
     * to the consequence and possibly throws an exception if the argument
     * is of the wrong format.
     */
    using Appliers = const std::map<std::string, std::function<Applier(const std::string&)>>;
    static Appliers appliers;

    std::string name;
    std::string value;
    Applier apply;

private:
    void applyTag(const Client* client, ClientChanges* changes) const;
    void applyIndex(const Client* client, ClientChanges* changes) const;
    void applyHook(const Client* client, ClientChanges* changes) const;
    void applyMonitor(const Client* client, ClientChanges* changes) const;
    static Consequence::Applier parseFloatingGeometry(const std::string&);
};

class Rule {
public:
    Rule();
    //! whether this rule should not be used anymore
    bool expired() {
        return expired_;
    };
    bool evaluate(Client* client, ClientChanges& changes, Output output);

    std::string label;
    std::vector<Condition> conditions;
    std::vector<Consequence> consequences;
    bool once = false;
    time_t birth_time; // timestamp of at creation

    bool setLabel(char op, std::string value, Output output);
    bool addCondition(const Condition::Matchers::const_iterator& it, char op, const char* value, bool negated, Output output);
    bool addConsequence(const Consequence::Appliers::const_iterator& it, const char* value, Output output);

    void print(Output output);
private:
    bool expired_ = false;
};

#endif
