#include "object.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <type_traits>

#include "arglist.h"
#include "attribute.h"
#include "entity.h"
#include "hook.h"
#include "utils.h"

using std::endl;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;

pair<ArgList,string> Object::splitPath(const string &path) {
    vector<string> splitpath = ArgList(path, OBJECT_PATH_SEPARATOR).toVector();
    if (splitpath.empty()) {
        return make_pair(splitpath, "");
    }
    string last = splitpath.back();
    splitpath.pop_back();
    return make_pair(splitpath, last);
}

void Object::wireAttributes(vector<Attribute*> attrs)
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
    if (it == attribs_.end()) {
        return;
    }
    if (it->second != attr) {
        return;
    }
    attribs_.erase(it);
}

void Object::wireActions(vector<Action*> actions)
{
    for (auto action : actions) {
        action->setOwner(this);
        actions_[action->name()] = action;
    }
}

void Object::ls(Output out)
{
    out << children_.size() << (children_.size() == 1 ? " child" : " children")
        << (!children_.empty() ? ":" : ".") << endl;
    for (auto it : children_) {
        out << "  " << it.first << "." << endl;
    }

    out << attribs_.size() << (attribs_.size() == 1 ? " attribute" : " attributes")
        << (!attribs_.empty() ? ":" : ".") << endl;

    out << " .---- type\n"
        << " | .-- writeable\n"
        << " | | .-- hookable\n"
        << " V V V" << endl;
    for (auto it : attribs_) {
        out << " " << it.second->typechar();
        out << " " << (it.second->writeable() ? "w" : "-");
        out << " " << (it.second->hookable() ? "h" : "-");
        out << " " << it.first;
        if (it.second->type() == Type::ATTRIBUTE_STRING
            || it.second->type() == Type::ATTRIBUTE_REGEX )
        {
            out << " = \"" << it.second->str() << "\"" << endl;
        } else {
            out << " = " << it.second->str() << endl;
        }
    }

    out << actions_.size() << (actions_.size() == 1 ? " action" : " actions")
        << (!actions_.empty() ? ":" : ".") << endl;
    for (auto it : actions_) {
        out << "  " << it.first << endl;
    }
}

Attribute* Object::attribute(const string &name) {
    auto it = attribs_.find(name);
    if (it == attribs_.end()) {
        return nullptr;
    } else {
        return it->second;
    }
}


Object* Object::child(const string &name) {
    auto it = children_.find(name);
    if (it != children_.end()) {
        return it->second;
    } else {
        return {};
    };
}


Object* Object::child(Path path) {
    std::ostringstream out;
    return child(path, out);
}

Object* Object::child(Path path, Output output) {
    Object* cur = this;
    string cur_path = "";
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

void Object::notifyHooks(HookEvent event, const string& arg)
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

void Object::addChild(Object* child, const string &name)
{
    children_[name] = child;
    notifyHooks(HookEvent::CHILD_ADDED, name);
}

void Object::removeChild(const string &child)
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
    //                [hook_locked](weak_ptr<Hook> el) {
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
    shared_ptr<TreeInterface> nthChild(size_t idx) override {
        return make_shared<DirectoryTreeInterface>(buf[idx].first, buf[idx].second);
    };
    void appendCaption(Output output) override {
        if (!lbl.empty()) {
            output << " " << lbl;
        }
    };
private:
    string lbl;
    vector<pair<string,Object*>> buf;
    Object* dir;
};

void Object::printTree(Output output, string rootLabel) {
    shared_ptr<TreeInterface> intface = make_shared<DirectoryTreeInterface>(rootLabel, this);
    tree_print_to(intface, output);
}

void Object::addStaticChild(Object* child, const string &name)
{
    children_[name] = child;
    notifyHooks(HookEvent::CHILD_ADDED, name);
}

Attribute* Object::deepAttribute(const string &path) {
    std::ostringstream output;
    return deepAttribute(path, output);
}

Attribute* Object::deepAttribute(const string &path, Output output) {
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

