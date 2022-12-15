#include "rules.h"

#include <algorithm>
#include <cstdio>

#include "client.h"
#include "ewmh.h"
#include "globals.h"
#include "hook.h"
#include "root.h"
#include "utils.h"
#include "xconnection.h"

using std::string;
using std::function;


/**
 * A helper function to define rule consequences that directly
 * write to a member of ClientConsequences of (plain) type T.
 *
 * This function returns an entry of the Consequence:appliers map.
 */
template <typename T>
function<Consequence::Applier(const string&)>
    setMember(T ClientChanges::* member)
{
    return [member](const string& valueStr) -> Consequence::Applier {
        T valueParsed = Converter<T>::parse(valueStr);
        return [member,valueParsed](const Consequence*, const Client*, ClientChanges* changes) -> void {
            (*changes).*member = valueParsed;
        };
    };
}

/**
 * A helper function to define rule consequences that directly
 * write to a member of ClientConsequences of type optional<T>
 *
 * This function returns an entry of the Consequence:appliers map.
 */
template <typename T>
function<Consequence::Applier(const string&)>
    setOptionalMember(std::experimental::optional<T> ClientChanges::* member)
{
    return [member](const string& valueStr) -> Consequence::Applier {
        T valueParsed = Converter<T>::parse(valueStr);
        return [member,valueParsed](const Consequence*, const Client*, ClientChanges* changes) -> void {
            (*changes).*member = valueParsed;
        };
    };
}


/// GLOBALS ///

const std::map<string, Condition::Matcher> Condition::matchers = {
    { "class",          &Condition::matchesClass             },
    { "instance",       &Condition::matchesInstance          },
    { "title",          &Condition::matchesTitle             },
    { "pid",            &Condition::matchesPid               },
    { "pgid",           &Condition::matchesPgid              },
    { "maxage",         &Condition::matchesMaxage            },
    { "windowtype",     &Condition::matchesWindowtype        },
    { "windowrole",     &Condition::matchesWindowrole        },
};

const std::map<string, function<Consequence::Applier(const string&)>> Consequence::appliers = {
    { "tag",            [] (const string&) { return &Consequence::applyTag; } },
    { "index",          [] (const string&) { return &Consequence::applyIndex; } },
    { "focus",          setMember(&ClientChanges::focus) },
    { "switchtag",      setMember(&ClientChanges::switchtag) },
    { "manage",         setMember(&ClientChanges::manage) },
    { "decorated",      setOptionalMember(&ClientChanges::decorated) },
    { "floating",       setOptionalMember(&ClientChanges::floating) },
    { "floating_geometry", parseFloatingGeometry },
    { "pseudotile",     setOptionalMember(&ClientChanges::pseudotile) },
    { "fullscreen",     setOptionalMember(&ClientChanges::fullscreen) },
    { "ewmhrequests",   setOptionalMember(&ClientChanges::ewmhRequests) },
    { "ewmhnotify",     setOptionalMember(&ClientChanges::ewmhNotify) },
    { "hook",           [] (const string&) { return &Consequence::applyHook; } },
    { "keymask",        setOptionalMember(&ClientChanges::keyMask) },
    { "keys_inactive",  setOptionalMember(&ClientChanges::keysInactive) },
    { "monitor",        [] (const string&) { return &Consequence::applyMonitor;        } },
    { "floatplacement", setMember(&ClientChanges::floatplacement) },
};

bool Rule::addCondition(const Condition::Matchers::const_iterator& it, char op, const char* value, bool negated, Output output) {
    Condition cond;
    cond.negated = negated;
    const string& name = it->first;

    cond.conditionCreationTime = get_monotonic_timestamp();

    if (op != '=' && name == "maxage") {
        output.perror() << "Condition maxage only supports the = operator\n";
        return false;
    }
    switch (op) {
        case '=': {
            if (name == "maxage") {
                cond.value_type = CONDITION_VALUE_TYPE_INTEGER;
                if (1 != sscanf(value, "%d", &cond.value_integer)) {
                    output.perror() << "Cannot parse integer from \"" << value << "\"\n";
                    return false;
                }
            } else {
                cond.value_type = CONDITION_VALUE_TYPE_STRING;
                cond.value_str = value;
            }
            break;
        }

        case '~': {
            cond.value_type = CONDITION_VALUE_TYPE_REGEX;
            try {
                cond.value_reg_exp = std::regex(value, std::regex::extended);
            } catch(std::regex_error& err) {
                output.perror() << "Cannot parse value \"" << value
                        << "\" from condition \"" << name
                        << "\": \"" << err.what() << "\"\n";
                return false;
            }
            cond.value_reg_str = value;
            break;
        }

        default:
            output.perror() << "Unknown rule condition operation \"" << op << "\"\n";
            return false;
            break;
    }

    cond.name = name;
    cond.match_ = it->second;

    conditions.push_back(cond);
    return true;
}

/**
 * Add consequence to this rule.
 *
 * @retval false if the consequence cannot be added (malformed)
 */
bool Rule::addConsequence(const Consequence::Appliers::const_iterator& it, const char* value, Output output) {
    Consequence cons;
    cons.name = it->first;
    cons.value = value;
    try {
        cons.apply = it->second(value);
    } catch(std::exception& err) {
        output.perror() << "Invalid argument \"" << value
                << "\" to consequence \"" << it->first
                << "\": " << err.what() << "\n";
        return false;
    }

    consequences.push_back(cons);
    return true;
}

bool Rule::setLabel(char op, string value, Output output) {
    if (op != '=') {
        output.perror() << "Unknown rule label operation \"" << op << "\"\n";
        return false;
    }

    if (value.empty()) {
        output.perror() << "Rule label cannot be empty\n";
        return false;
    }

    label = value;
    return true;
}

// rules parsing //

Rule::Rule() {
    birth_time = get_monotonic_timestamp();
}

/**
 * @brief apply the rule to a client and return whether the rule matched
 * @param the client to apply the rules to
 * @param the resulting changes
 * @return whether the rule matched.
 */
bool Rule::evaluate(Client* client, ClientChanges& changes, Output output)
{
    bool rule_match = true; // if entire rule matches

    // check all conditions
    for (auto& cond : conditions) {
        if (!rule_match && cond.name != "maxage") {
            // implement lazy AND &&
            // ... except for maxage
            continue;
        }

        bool matches = cond.match_(&cond, client);

        if (!matches && !cond.negated && cond.name == "maxage")
        {
            // if not negated maxage does not match anymore
            // then it will never match again in the future
            expired_ = true;
        }

        if (cond.negated) {
            matches = ! matches;
        }
        rule_match = rule_match && matches;
    }

    if (rule_match) {
        // apply all consequences
        for (auto& cons : consequences) {
            try {
                cons.apply(&cons, client, &changes);
            } catch (std::exception& e) {
                output.error()
                       << "Invalid argument \"" << cons.value
                       << "\" for rule consequence \"" << cons.name << "\": "
                       << e.what() << "\n";
            }
        }
    }
    if (rule_match && once) {
        expired_ = true;
    }
    return rule_match;
}

void Rule::print(Output output) {
    output << "label=" << label << "\t";

    // Append conditions
    for (auto const& cond : conditions) {
        if (cond.negated) {
            output << "not\t";
        }
        output << cond.name;
        switch (cond.value_type) {
            case CONDITION_VALUE_TYPE_STRING:
                output << "=" << cond.value_str << "\t";
                break;
            case CONDITION_VALUE_TYPE_REGEX:
                output << "~" << cond.value_reg_str << "\t";
                break;
            default: /* CONDITION_VALUE_TYPE_INTEGER: */
                output << "=" << cond.value_integer << "\t";
        }
    }

    // Append consequences
    for (auto const& cons : consequences) {
        output << cons.name << "=" << cons.value << "\t";
    }

    // Append separating or final newline
    output << '\n';
}

// rules applying //
ClientChanges::ClientChanges()
{
}

/// CONDITIONS ///
bool Condition::matches(const string& str) const {
    switch (value_type) {
        case CONDITION_VALUE_TYPE_STRING:
            return value_str == str;
            break;
        case CONDITION_VALUE_TYPE_REGEX:
            return std::regex_match(str, value_reg_exp);
            break;
        case CONDITION_VALUE_TYPE_INTEGER:
            try {
                return std::stoi(str) == value_integer;
            } catch (std::exception&) {
                return false;
            }
            break;
    }
    return false;
}

bool Condition::matchesClass(const Client* client) const {
    return matches(Root::get()->X.getClass(client->window_));
}

bool Condition::matchesInstance(const Client* client) const {
    return matches(Root::get()->X.getInstance(client->window_));
}

bool Condition::matchesTitle(const Client* client) const {
    return matches(client->title_());
}

bool Condition::matchesPid(const Client* client) const {
    if (client->pid_() < 0) {
        return false;
    }
    if (value_type == CONDITION_VALUE_TYPE_INTEGER) {
        return value_integer == client->pid_;
    } else {
        char buf[1000]; // 1kb ought to be enough for every int
        sprintf(buf, "%d", client->pid_());
        return matches(buf);
    }
}

bool Condition::matchesPgid(const Client* client) const {
    if (client->pgid_() < 0) {
        return false;
    }
    if (value_type == CONDITION_VALUE_TYPE_INTEGER) {
        return value_integer == client->pgid_;
    } else {
        char buf[1000]; // 1kb ought to be enough for every int
        sprintf(buf, "%d", client->pgid_());
        return matches(buf);
    }
}

bool Condition::matchesMaxage(const Client* client) const {
    time_t diff = get_monotonic_timestamp() - conditionCreationTime;
    return (value_integer >= diff);
}

bool Condition::matchesWindowtype(const Client* client) const {
    auto& ewmh = Ewmh::get();
    int wintype = ewmh.getWindowType(client->x11Window());
    if (wintype < 0) {
        return false;
    }
    return matches(ewmh.netatomName(wintype));
}

bool Condition::matchesWindowrole(const Client* client) const {
    auto& X = Root::get()->X;
    auto role = X.getWindowProperty(client->window_, X.atom("WM_WINDOW_ROLE"));

    if (!role.has_value()) {
        return false;
    }

    return matches(role.value());
}

bool Condition::matchesFixedSize(const Client* client) const {
    return client->maxw_ != 0
            && client->maxh_ != 0
            && client->minh_ == client->maxh_
            && client->minw_ == client->maxw_;
}

/// CONSEQUENCES ///
void Consequence::applyTag(const Client* client, ClientChanges* changes) const {
    changes->tag_name = value;
}

void Consequence::applyIndex(const Client* client, ClientChanges* changes) const {
    changes->tree_index = value;
}

void Consequence::applyHook(const Client* client, ClientChanges* changes) const {
    hook_emit({ "rule", value, WindowID(client->window_).str() });
}

void Consequence::applyMonitor(const Client* client, ClientChanges* changes) const {
    changes->monitor_name = value;
}

Consequence::Applier Consequence::parseFloatingGeometry(const string& source)
{
    Rectangle geo = Converter<Rectangle>::parse(source);
    if (geo.width < WINDOW_MIN_WIDTH || geo.height < WINDOW_MIN_HEIGHT) {
        throw std::invalid_argument("Rectangle too small");
    }
    return [geo](const Consequence*, const Client*, ClientChanges* changes) {
        changes->floatingGeometry = geo;
    };
}

template<>
Finite<ClientPlacement>::ValueList Finite<ClientPlacement>::values = ValueListPlain {
    { ClientPlacement::Center, "center" },
    { ClientPlacement::Unchanged, "none" },
    { ClientPlacement::Smart, "smart" },
};
