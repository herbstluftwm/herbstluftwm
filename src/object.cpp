/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "object.h"
#include "command.h"
#include "utils.h"
#include "assert.h"
#include "globals.h"
#include "ipc-protocol.h"

#include <iostream>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sstream>

namespace herbstluft {

Object::Object(const std::string &name)
    : Directory(name),
      nameAttribute_("name", Type::ATTRIBUTE_STRING, false, true)
{
    wireAttributes({ &nameAttribute_ });
}


bool Object::exists(const std::string &name, Type t)
{
    switch (t) {
    case Type::DIRECTORY: return Directory::exists(name);
    case Type::ATTRIBUTE: return attribs_.find(name) != attribs_.end();
    case Type::ACTION: return actions_.find(name) != actions_.end();
    default: return false; // TODO: throw
    }
}

std::string Object::read(const std::string &attr) const {
    if (attr == "name")
        return name_;

    auto it = attribs_.find(attr);
    if (it != attribs_.end())
        return it->second->str();
    return {}; // TODO: throw
}

bool Object::writeable(const std::string &attr) const {
    auto it = attribs_.find(attr);
    if (it != attribs_.end()) {
        return it->second->writeable();
    }
    return false; // TODO: throw
}

void Object::write(const std::string &attr, const std::string &value) {
    auto it = attribs_.find(attr);
    if (it != attribs_.end()) {
        if (it->second->writeable())
            it->second->change(value);
        // TODO: else throw
    } else {
        // TODO: throw
    }
}

bool Object::hookable(const std::string &attr) const {
    auto it = attribs_.find(attr);
    if (it != attribs_.end()) {
        return it->second->hookable();
    }
    return false; // TODO: else throw
}

void Object::trigger(const std::string &action, ArgList args) {
    // do nothing, there is no default behavior for actions.
    // TODO: throw; if we got here, there was an error, e.g. typo on user's side
}

void Object::wireAttributes(std::vector<Attribute*> attrs)
{
    for (auto attr : attrs) {
        attr->setOwner(this);
        attribs_[attr->name()] = attr;
    }
}

void Object::wireActions(std::vector<Action*> actions)
{
    for (auto action : actions) {
        action->setOwner(this);
        actions_[action->name()] = action;
    }
}

void Object::ls(Output out)
{
    Directory::ls(out);

    out << attribs_.size() << " attributes"
        << (attribs_.size() > 0 ? ":" : ".") << std::endl;
    for (auto it : attribs_) {
        out << "  " << it.first << "." << std::endl;
    }

    out << actions_.size() << " actions"
        << (actions_.size() > 0 ? ":" : ".") << std::endl;
    for (auto it : actions_) {
        out << "  " << it.first << "." << std::endl;
    }
}

void Object::print(const std::string &prefix)
{
    std::cout << prefix << "==== " << typestr() << " " << name_ << ":" << std::endl;
    if (!children_.empty()) {
        std::cout << prefix << "Children:" << std::endl;
        for (auto it : children_) {
            it.second->print(prefix + "\t| ");
        }
        std::cout << prefix << std::endl;
    }
    if (!attribs_.empty()) {
        std::cout << prefix << "Attributes:" << std::endl;
        for (auto it : attribs_) {
            std::cout << prefix << "\t" << it.first
                      << " (" << it.second->typestr() << ")";
            std::cout << "\t[" << read(it.second->name()) << "]";
            if (it.second->writeable())
                std::cout << "\tw";
            if (!it.second->hookable())
                std::cout << "\t!h";
            std::cout << std::endl;
        }
    }
    if (!actions_.empty()) {
        std::cout << prefix << "Actions:" << std::endl;
        std::cout << prefix;
        for (auto it : actions_) {
            std::cout << "\t" << it.first;
        }
        std::cout << std::endl;
    }
    if (!hooks_.empty()) {
        std::cout << prefix << "Current hooks:" << std::endl;
        for (auto it : hooks_) {
            std::cout << prefix << "\t" << it.first << std::endl;
        }
    }
}

}


