#include "rules.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <algorithm>
#include <cstdio>

#include "globals.h"
#include "utils.h"
#include "ewmh.h"
#include "client.h"
#include "hook.h"

using std::string;

/// GLOBALS ///

const std::map<string, Condition::Matcher> Condition::matchers = {
    { "class",          &Condition::matchesClass             },
    { "instance",       &Condition::matchesInstance          },
    { "title",          &Condition::matchesTitle             },
    { "pid",            &Condition::matchesPid               },
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
    { "pseudotile",     &Consequence::applyPseudotile      },
    { "fullscreen",     &Consequence::applyFullscreen      },
    { "ewmhrequests",   &Consequence::applyEwmhrequests    },
    { "ewmhnotify",     &Consequence::applyEwmhnotify      },
    { "hook",           &Consequence::applyHook            },
    { "keymask",        &Consequence::applyKeyMask         },
    { "monitor",        &Consequence::applyMonitor         },
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
ClientChanges::ClientChanges(Client *client)
    : fullscreen(ewmh_is_fullscreen_set(client->window_))
{}

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
    auto window_class = window_class_to_string(g_display, client->window_);
    return matches(window_class);
}

bool Condition::matchesInstance(const Client* client) const {
    auto inst = window_instance_to_string(g_display, client->window_);
    return matches(inst);
}

bool Condition::matchesTitle(const Client* client) const {
    return matches(client->title_());
}

bool Condition::matchesPid(const Client* client) const {
    if (client->pid_ < 0) {
        return false;
    }
    if (value_type == CONDITION_VALUE_TYPE_INTEGER) {
        return value_integer == client->pid_;
    } else {
        char buf[1000]; // 1kb ought to be enough for every int
        sprintf(buf, "%d", client->pid_);
        return matches(buf);
    }
}

bool Condition::matchesMaxage(const Client* client) const {
    time_t diff = get_monotonic_timestamp() - conditionCreationTime;
    return (value_integer >= diff);
}

bool Condition::matchesWindowtype(const Client* client) const {
    // that only works for atom-type utf8-string, _NET_WM_WINDOW_TYPE is int
    //  GString* wintype=
    //      window_property_to_g_string(g_display, client->window, wintype_atom);
    // =>
    // kinda duplicate from src/utils.c:window_properties_to_g_string()
    // using different xatom type, and only calling XGetWindowProperty
    // once, because we are sure we only need four bytes
    long bufsize = 10;
    char *buf;
    Atom type_ret, wintype;
    int format;
    unsigned long items, bytes_left;
    long offset = 0;

    int status = XGetWindowProperty(
            g_display,
            client->window_,
            g_netatom[NetWmWindowType],
            offset,
            bufsize,
            False,
            ATOM("ATOM"),
            &type_ret,
            &format,
            &items,
            &bytes_left,
            (unsigned char**)&buf
            );
    // we only need precisely four bytes (one Atom)
    // if there are bytes left, something went wrong
    if(status != Success || bytes_left > 0 || items < 1 || buf == nullptr) {
        return false;
    } else {
        wintype= *(Atom *)buf;
        XFree(buf);
    }

    for (int i = NetWmWindowTypeFIRST; i <= NetWmWindowTypeLAST; i++) {
        // try to find the window type
        if (wintype == g_netatom[i]) {
            // if found, then treat the window type as a string value,
            // which is registered in g_netatom_names[]
            return matches(g_netatom_names[i]);
        }
    }

    // if no valid window type has been found,
    // it can not match
    return false;
}

bool Condition::matchesWindowrole(const Client* client) const {
    auto role = window_property_to_string(g_display, client->window_,
        ATOM("WM_WINDOW_ROLE"));

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
    changes->focus = string_to_bool(value, changes->focus);
}

void Consequence::applyManage(const Client* client, ClientChanges* changes) const {
    changes->manage = string_to_bool(value, changes->manage);
}

void Consequence::applyIndex(const Client* client, ClientChanges* changes) const {
    changes->tree_index = value;
}

void Consequence::applyPseudotile(const Client* client, ClientChanges* changes) const {
    changes->pseudotile = string_to_bool(value, client->pseudotile_);
}

void Consequence::applyFullscreen(const Client* client, ClientChanges* changes) const {
    changes->fullscreen = string_to_bool(value, changes->fullscreen);
}

void Consequence::applySwitchtag(const Client* client, ClientChanges* changes) const {
    changes->switchtag = string_to_bool(value, changes->switchtag);
}

void Consequence::applyEwmhrequests(const Client* client, ClientChanges* changes) const {
    changes->ewmhRequests = string_to_bool(value, client->ewmhrequests_);
}

void Consequence::applyEwmhnotify(const Client* client, ClientChanges* changes) const {
    changes->ewmhNotify = string_to_bool(value, client->ewmhnotify_);
}

void Consequence::applyHook(const Client* client, ClientChanges* changes) const {
    std::stringstream winidSs;
    winidSs << "0x" << std::hex << client->window_;
    auto winidStr = winidSs.str();
    const char* hook_str[] = { "rule", value.c_str(), winidStr.c_str() };
    hook_emit(LENGTH(hook_str), hook_str);
}

void Consequence::applyKeyMask(const Client* client, ClientChanges* changes) const {
    changes->keyMask = value;
}

void Consequence::applyMonitor(const Client* client, ClientChanges* changes) const {
    changes->monitor_name = value;
}
