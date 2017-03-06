#include "root.h"
#include "tagmanager.h"
#include "hookmanager.h"
#include "clientmanager.h"
#include "attribute.h"
#include "ipc-protocol.h"
#include "globals.h"

#include <memory>
#include <stdexcept>
#include <sstream>

std::shared_ptr<Root> Root::root_;

Root::Root()
    : clients(*this, "clients")
    , tags(*this, "tags")
    , hooks(*this, "hooks")
{
    clients = std::make_shared<ClientManager>();
    tags = std::make_shared<TagManager>();
    hooks = std::make_shared<HookManager>();

    // set temporary globals
    ::tags = tags();
}

Root::~Root()
{
    clients = {};
    tags = {};
    children_.clear(); // avoid possible circular shared_ptr dependency
}

int Root::cmd_get_attr(Input in, Output output) {
    in.shift();
    if (in.empty()) return HERBST_NEED_MORE_ARGS;

    Attribute* a = getAttribute(in.front(), output);
    if (!a) return HERBST_INVALID_ARGUMENT;
    output << a->str();
    return 0;
}

int Root::cmd_attr(Input in, Output output) {
    in.shift();

    auto path = in.empty() ? std::string("") : in.front();
    in.shift();
    std::ostringstream dummy_output;
    std::shared_ptr<Object> o = shared_from_this();
    auto p = Path::split(path);
    if (!p.empty()) {
        while (p.back().empty()) p.pop_back();
        o = o->child(p);
    }
    if (o && in.empty()) {
        o->ls(output);
        return 0;
    }

    Attribute* a = getAttribute(path, output);
    if (!a) return HERBST_INVALID_ARGUMENT;
    if (in.empty()) {
        // no more arguments -> return the value
        output << a->str();
        return 0;
    } else {
        // another argument -> set the value
        std::string error_message = a->change(in.front());
        if (error_message == "") {
            return 0;
        } else {
            output << error_message << std::endl;
            return HERBST_INVALID_ARGUMENT;
        }
    }
}

Attribute* Root::getAttribute(std::string path, Output output) {
    auto attr_path = Object::splitPath(path);
    auto child = Root::get()->child(attr_path.first);
    if (!child) {
        output << "No such object " << attr_path.first.join('.') << std::endl;
        return NULL;
    }
    Attribute* a = child->attribute(attr_path.second);
    if (!a) {
        output << "Object " << attr_path.first.join('.')
               << " has no attribute \"" << attr_path.second << "\""
               << std::endl;
        return NULL;
    }
    return a;
}

int print_object_tree_command(ArgList in, Output output) {
    in.shift();
    auto path = Path(in.empty() ? std::string("") : in.front()).toVector();
    while (!path.empty() && path.back() == "") {
        path.pop_back();
    }
    auto child = Root::get()->child(path);
    if (!child) {
        output << "No such object " << Path(path).join('.') << std::endl;
        return HERBST_INVALID_ARGUMENT;
    }
    child->printTree(output, Path(path).join('.'));
    return 0;
}

