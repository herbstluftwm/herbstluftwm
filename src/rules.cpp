#include "rules.h"

#include <algorithm>
#include <cstdio>

#include "client.h"
#include "ewmh.h"
#include "finite.h"
#include "hook.h"
#include "root.h"
#include "utils.h"
#include "xconnection.h"

using std::string;

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

const std::map<string, Consequence::Applier> Consequence::appliers = {
    { "tag",            &Consequence::applyTag             },
    { "index",          &Consequence::applyIndex           },
    { "focus",          &Consequence::applyFocus           },
    { "switchtag",      &Consequence::applySwitchtag       },
    { "manage",         &Consequence::applyManage          },
    { "floating",       &Consequence::applyFloating        },
    { "pseudotile",     &Consequence::applyPseudotile      },
    { "fullscreen",     &Consequence::applyFullscreen      },
    { "ewmhrequests",   &Consequence::applyEwmhrequests    },
    { "ewmhnotify",     &Consequence::applyEwmhnotify      },
    { "hook",           &Consequence::applyHook            },
    { "keymask",        &Consequence::applyKeyMask         },
    { "keys_inactive",  &Consequence::applyKeysInactive    },
    { "monitor",        &Consequence::applyMonitor         },
    { "floatplacement", &Consequence::applyFloatplacement       },
};

bool Rule::addCondition(string name, char op, const char* value, bool negated, Output output) {
    Condition cond;
    cond.negated = negated;

    cond.conditionCreationTime = get_monotonic_timestamp();

    if (op != '=' && name == "maxage") {
        output << "rule: Condition maxage only supports the = operator\n";
        return false;
    }
    switch (op) {
        case '=': {
            if (name == "maxage") {
                cond.value_type = CONDITION_VALUE_TYPE_INTEGER;
                if (1 != sscanf(value, "%d", &cond.value_integer)) {
                    output << "rule: Can not integer from \"" << value << "\"\n";
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
                output << "rule: Can not parse value \"" << value
                        << "\" from condition \"" << name
                        << "\": \"" << err.what() << "\"\n";
                return false;
            }
            cond.value_reg_str = value;
            break;
        }

        default:
            output << "rule: Unknown rule condition operation \"" << op << "\"\n";
            return false;
            break;
    }

    cond.name = name;

    conditions.push_back(cond);
    return true;
}

/**
 * Add consequence to this rule.
 *
 * @retval false if the consequence cannot be added (malformed)
 */
bool Rule::addConsequence(string name, char op, const char* value, Output output) {
    Consequence cons;
    switch (op) {
        case '=':
            cons.value_type = CONSEQUENCE_VALUE_TYPE_STRING;
            cons.value = value;
            break;

        default:
            output << "rule: Unknown rule consequence operation \"" << op << "\"\n";
            return false;
            break;
    }

    cons.name = name;

    consequences.push_back(cons);
    return true;
}

bool Rule::setLabel(char op, string value, Output output) {
    if (op != '=') {
        output << "rule: Unknown rule label operation \"" << op << "\"\n";
        return false;
    }

    if (value.empty()) {
        output << "rule: Rule label cannot be empty\n";
        return false;
    }

    label = value;
    return true;
}

// rules parsing //

Rule::Rule() {
    birth_time = get_monotonic_timestamp();
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

/// CONSEQUENCES ///
void Consequence::applyTag(const Client* client, ClientChanges* changes) const {
    changes->tag_name = value;
}

void Consequence::applyFocus(const Client* client, ClientChanges* changes) const {
    changes->focus = Converter<bool>::parse(value, changes->focus);
}

void Consequence::applyManage(const Client* client, ClientChanges* changes) const {
    changes->manage = Converter<bool>::parse(value, changes->manage);
}

void Consequence::applyFloating(const Client *client, ClientChanges *changes) const
{
    changes->floating = Converter<bool>::parse(value, client->floating_);
}

void Consequence::applyIndex(const Client* client, ClientChanges* changes) const {
    changes->tree_index = value;
}

void Consequence::applyPseudotile(const Client* client, ClientChanges* changes) const {
    changes->pseudotile = Converter<bool>::parse(value, client->pseudotile_);
}

void Consequence::applyFullscreen(const Client* client, ClientChanges* changes) const {
    changes->fullscreen = Converter<bool>::parse(value);
}

void Consequence::applySwitchtag(const Client* client, ClientChanges* changes) const {
    changes->switchtag = Converter<bool>::parse(value, changes->switchtag);
}

void Consequence::applyEwmhrequests(const Client* client, ClientChanges* changes) const {
    changes->ewmhRequests = Converter<bool>::parse(value, client->ewmhrequests_);
}

void Consequence::applyEwmhnotify(const Client* client, ClientChanges* changes) const {
    changes->ewmhNotify = Converter<bool>::parse(value, client->ewmhnotify_);
}

void Consequence::applyHook(const Client* client, ClientChanges* changes) const {
    hook_emit({ "rule", value, WindowID(client->window_).str() });
}

void Consequence::applyKeyMask(const Client* client, ClientChanges* changes) const {
    changes->keyMask = Converter<RegexStr>::parse(value);
}

void Consequence::applyKeysInactive(const Client *client, ClientChanges *changes) const
{
    changes->keysInactive = Converter<RegexStr>::parse(value);
}

void Consequence::applyMonitor(const Client* client, ClientChanges* changes) const {
    changes->monitor_name = value;
}

void Consequence::applyFloatplacement(const Client* client, ClientChanges* changes) const {
    changes->floatplacement = Converter<ClientPlacement>::parse(value);
}

template<>
Finite<ClientPlacement>::ValueList Finite<ClientPlacement>::values = {
    { ClientPlacement::Center, "center" },
    { ClientPlacement::Unchanged, "none" },
};

template<>
string Converter<ClientPlacement>::str(ClientPlacement cp) {
    return Finite<ClientPlacement>::str(cp);
}

template<>
ClientPlacement Converter<ClientPlacement>::parse(const string& payload) {
    return Finite<ClientPlacement>::parse(payload);
}

template<>
void Converter<ClientPlacement>::complete(Completion& complete, ClientPlacement const* relativeTo) {
    return Finite<ClientPlacement>::complete(complete, relativeTo);
}


