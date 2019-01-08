#include "rules.h"
#include "globals.h"
#include "utils.h"
#include "ewmh.h"
#include "client.h"
#include "ipc-protocol.h"
#include "hook.h"

#include <cstring>
#include <cstdio>
#include <sys/types.h>
#include <list>
#include <algorithm>

/// TYPES ///

typedef struct {
    const char*   name;
    bool    (*matches)(HSCondition* condition, HSClient* client);
} HSConditionType;

typedef struct {
    const char*   name;
    void    (*apply)(HSConsequence* consequence, HSClient* client,
                     HSClientChanges* changes);
} HSConsequenceType;


/// GLOBALS ///

const std::map<std::string, std::function<bool(HSCondition * ,HSClient*)>> HSCondition::matchers = {
    { "class",          &HSCondition::matchesClass             },
    { "instance",       &HSCondition::matchesInstance          },
    { "title",          &HSCondition::matchesTitle             },
    { "pid",            &HSCondition::matchesPid               },
    { "maxage",         &HSCondition::matchesMaxage            },
    { "windowtype",     &HSCondition::matchesWindowtype        },
    { "windowrole",     &HSCondition::matchesWindowrole        },
};

const std::map<std::string, std::function<void(HSConsequence*, HSClient*, HSClientChanges*)>> HSConsequence::appliers = {
    { "tag",            &HSConsequence::applyTag             },
    { "index",          &HSConsequence::applyIndex           },
    { "focus",          &HSConsequence::applyFocus           },
    { "switchtag",      &HSConsequence::applySwitchtag       },
    { "manage",         &HSConsequence::applyManage          },
    { "pseudotile",     &HSConsequence::applyPseudotile      },
    { "fullscreen",     &HSConsequence::applyFullscreen      },
    { "ewmhrequests",   &HSConsequence::applyEwmhrequests    },
    { "ewmhnotify",     &HSConsequence::applyEwmhnotify      },
    { "hook",           &HSConsequence::applyHook            },
    { "keymask",        &HSConsequence::applyKeymask         },
    { "monitor",        &HSConsequence::applyMonitor         },
};

bool HSRule::addCondition(std::string name, char op, const char* value, bool negated, Output output) {
    HSCondition cond;
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
bool HSRule::addConsequence(std::string name, char op, const char* value, Output output) {
    HSConsequence cons;
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

bool HSRule::setLabel(char op, std::string value, Output output) {
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

HSRule::HSRule() {
    birth_time = get_monotonic_timestamp();
}

void HSRule::print(Output output) {
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
HSClientChanges::HSClientChanges(HSClient *client)
    : fullscreen(ewmh_is_fullscreen_set(client->window_))
{}

/// CONDITIONS ///
bool HSCondition::matches(const std::string& string) {
    switch (value_type) {
        case CONDITION_VALUE_TYPE_STRING:
            return value_str == string;
            break;
        case CONDITION_VALUE_TYPE_REGEX:
            return std::regex_match(string, value_reg_exp);
            break;
        case CONDITION_VALUE_TYPE_INTEGER:
            try {
                return std::stoi(string) == value_integer;
            } catch (std::exception&) {
                return false;
            }
            break;
    }
    return false;
}

bool HSCondition::matchesClass(HSClient* client) {
    GString* window_class = window_class_to_g_string(g_display, client->window_);
    bool match = matches(window_class->str);
    g_string_free(window_class, true);
    return match;
}

bool HSCondition::matchesInstance(HSClient* client) {
    GString* inst = window_instance_to_g_string(g_display, client->window_);
    bool match = matches(inst->str);
    g_string_free(inst, true);
    return match;
}

bool HSCondition::matchesTitle(HSClient* client) {
    return matches(client->title_());
}

bool HSCondition::matchesPid(HSClient* client) {
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

bool HSCondition::matchesMaxage(HSClient* client) {
    time_t diff = get_monotonic_timestamp() - conditionCreationTime;
    return (value_integer >= diff);
}

bool HSCondition::matchesWindowtype(HSClient* client) {
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

bool HSCondition::matchesWindowrole(HSClient* client) {
    GString* role = window_property_to_g_string(g_display, client->window_,
        ATOM("WM_WINDOW_ROLE"));
    if (!role) return false;
    bool match = matches(role->str);
    g_string_free(role, true);
    return match;
}

/// CONSEQUENCES ///
void HSConsequence::applyTag(HSClient* client, HSClientChanges* changes) {
    changes->tag_name = value;
}

void HSConsequence::applyFocus(HSClient* client, HSClientChanges* changes) {
    changes->focus = string_to_bool(value, changes->focus);
}

void HSConsequence::applyManage(HSClient* client, HSClientChanges* changes) {
    changes->manage = string_to_bool(value, changes->manage);
}

void HSConsequence::applyIndex(HSClient* client, HSClientChanges* changes) {
    changes->tree_index = value;
}

void HSConsequence::applyPseudotile(HSClient* client, HSClientChanges* changes) {
    client->pseudotile_ = string_to_bool(value, client->pseudotile_);
}

void HSConsequence::applyFullscreen(HSClient* client, HSClientChanges* changes) {
    changes->fullscreen = string_to_bool(value, changes->fullscreen);
}

void HSConsequence::applySwitchtag(HSClient* client, HSClientChanges* changes) {
    changes->switchtag = string_to_bool(value, changes->switchtag);
}

void HSConsequence::applyEwmhrequests(HSClient* client, HSClientChanges* changes) {
    // this is only a flag that is unused during initialization (during
    // manage()) and so can be directly changed in the client
    client->ewmhrequests_ = string_to_bool(value, client->ewmhrequests_);
}

void HSConsequence::applyEwmhnotify(HSClient* client, HSClientChanges* changes) {
    client->ewmhnotify_ = string_to_bool(value, client->ewmhnotify_);
}

void HSConsequence::applyHook(HSClient* client, HSClientChanges* changes) {
    GString* winid = g_string_sized_new(20);
    g_string_printf(winid, "0x%lx", client->window_);
    const char* hook_str[] = { "rule" , value.c_str(), winid->str };
    hook_emit(LENGTH(hook_str), hook_str);
    g_string_free(winid, true);
}

void HSConsequence::applyKeymask(HSClient* client, HSClientChanges* changes) {
    changes->keymask = value;
}

void HSConsequence::applyMonitor(HSClient* client, HSClientChanges* changes) {
    changes->monitor_name = value;
}
