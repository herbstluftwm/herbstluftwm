#include "object.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>

#include "arglist.h"
#include "utils.h"
#include "cassert"
#include "globals.h"
#include "hook.h"
#include "attribute.h"

using namespace std;

std::string Object::read(const std::string &attr) const {
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

std::pair<ArgList,std::string> Object::splitPath(const std::string &path) {
    std::vector<std::string> splitpath = ArgList(path, OBJECT_PATH_SEPARATOR).toVector();
    if (splitpath.empty()) {
        return make_pair(splitpath, "");
    }
    std::string last = splitpath.back();
    splitpath.pop_back();
    return make_pair(splitpath, last);
}

void Object::wireAttributes(std::vector<Attribute*> attrs)
{
    for (auto attr : attrs) {
        attr->setOwner(this);
        attribs_[attr->name()] = attr;
    }
}

void Object::addAttribute(Attribute* attr) {
    attr->setOwner(this);
    attribs_[attr->name()] = attr;
}

void Object::removeAttribute(Attribute* attr) {
    auto it = attribs_.find(attr->name());
    if (it == attribs_.end()) return;
    if (it->second != attr) return;
    attribs_.erase(it);
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
    out << children_.size() << (children_.size() == 1 ? " child" : " children")
        << (children_.size() > 0 ? ":" : ".") << std::endl;
    for (auto it : children_) {
        out << "  " << it.first << "." << std::endl;
    }

    out << attribs_.size() << (attribs_.size() == 1 ? " attribute" : " attributes")
        << (attribs_.size() > 0 ? ":" : ".") << std::endl;

    out << " .---- type\n"
        << " | .-- writeable\n"
        << " | | .-- hookable\n"
        << " V V V" << std::endl;
    for (auto it : attribs_) {
        out << " " << it.second->typechar();
        out << " " << (it.second->writeable() ? "w" : "-");
        out << " " << (it.second->hookable() ? "h" : "-");
        out << " " << it.first;
        if (it.second->type() == Type::ATTRIBUTE_STRING) {
            out << " = \"" << it.second->str() << "\"" << std::endl;
        } else {
            out << " = " << it.second->str() << std::endl;
        }
    }

    out << actions_.size() << (actions_.size() == 1 ? " action" : " actions")
        << (actions_.size() > 0 ? ":" : ".") << std::endl;
    for (auto it : actions_) {
        out << "  " << it.first << std::endl;
    }
}
void Object::ls(Path path, Output out) {
    if (path.empty())
        return ls(out);

    auto child = path.front();
    if (children_.find(child) != children_.end()) {
        path.shift();
        children_[child]->ls(path, out);
    } else {
        out << "child " << child << " not found!" << std::endl; // TODO
    }
}

void Object::print(const std::string &prefix)
{
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
            std::cout << "\t[" << it.second->str() << "]";
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
    std::cout << prefix << "Currently " << hooks_.size() << " hooks:" << std::endl;
}

Attribute* Object::attribute(const std::string &name) {
    auto it = attribs_.find(name);
    if (it == attribs_.end()) {
        return nullptr;
    } else {
        return it->second;
    }
}


Object* Object::child(const std::string &name) {
    auto it = children_.find(name);
    if (it != children_.end())
        return it->second;
    else
        return {};
}


Object* Object::child(Path path) {
    std::ostringstream out;
    return child(path, out);
}

Object* Object::child(Path path, Output output) {
    Object* cur = this;
    std::string cur_path = "";
    while (!path.empty()) {
        auto it = cur->children_.find(path.front());
        if (it != cur->children_.end()) {
            cur = it->second;
            cur_path += path.front();
            cur_path += ".";
        } else {
            output << "Object \"" << cur_path << "\""
                << " has no child named \"" << path.front()
                << "\"" << endl;
            return nullptr;
        }
        path.shift();
    }
    return cur;
}

void Object::notifyHooks(HookEvent event, const std::string& arg)
{
    for (auto h : hooks_) {
        if (h) {
            switch (event) {
                case HookEvent::CHILD_ADDED:
                    h->childAdded(this, arg);
                    break;
                case HookEvent::CHILD_REMOVED:
                    h->childRemoved(this, arg);
                    break;
                case HookEvent::ATTRIBUTE_CHANGED:
                    h->attributeChanged(this, arg);
                    break;
            }
        } // TODO: else throw
    }
}

void Object::addChild(Object* child, const std::string &name)
{
    children_[name] = child;
    notifyHooks(HookEvent::CHILD_ADDED, name);
}

void Object::removeChild(const std::string &child)
{
    notifyHooks(HookEvent::CHILD_REMOVED, child);
    children_.erase(child);
}


void Object::addHook(Hook* hook)
{
    hooks_.push_back(hook);
}

void Object::removeHook(Hook* hook)
{
    //auto hook_locked = hook.lock();
    //hooks_.erase(std::remove_if(
    //                hooks_.begin(),
    //                hooks_.end(),
    //                [hook_locked](std::weak_ptr<Hook> el) {
    //                    return el.lock() == hook_locked;
    //                }), hooks_.end());
    hooks_.erase(std::remove(
                    hooks_.begin(),
                    hooks_.end(),
                    hook), hooks_.end());
}

class DirectoryTreeInterface : public TreeInterface {
public:
    DirectoryTreeInterface(string label, Object* d) : lbl(label), dir(d) {
        for (auto child : dir->children()) {
            buf.push_back(child);
        }
    };
    size_t childCount() override {
        return buf.size();
    };
    Ptr(TreeInterface) nthChild(size_t idx) override {
        return make_shared<DirectoryTreeInterface>(buf[idx].first, buf[idx].second);
    };
    void appendCaption(Output output) override {
        output << lbl;
    };
private:
    string lbl;
    vector<pair<string,Object*>> buf;
    Object* dir;
};

void Object::printTree(Output output, std::string rootLabel) {
    Ptr(TreeInterface) intface = make_shared<DirectoryTreeInterface>(rootLabel, this);
    tree_print_to(intface, output);
}

void Object::addStaticChild(Object* child, const std::string &name)
{
    children_[name] = child;
    notifyHooks(HookEvent::CHILD_ADDED, name);
}

Attribute* Object::deepAttribute(const std::string &path) {
    std::ostringstream output;
    return deepAttribute(path, output);
}

Attribute* Object::deepAttribute(const std::string &path, Output output) {
    auto attr_path = splitPath(path);
    auto attribute_owner = child(attr_path.first, output);
    if (!attribute_owner) {
        return nullptr;
    }
    Attribute* a = attribute_owner->attribute(attr_path.second);
    if (!a) {
        output << "Object \"" << attr_path.first.join()
            << "\" has no attribute \"" << attr_path.second
            << "\"."
            << endl;
        return nullptr;
    }
    return a;
}

