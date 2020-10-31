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
    const auto& children = this->children();
    out << children.size() << (children.size() == 1 ? " child" : " children")
        << (!children.empty() ? ":" : ".") << endl;
    for (auto it : children) {
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
void Object::ls(Path path, Output out) {
    if (path.empty()) {
        return ls(out);
    }

    auto child = path.front();
    if (children_.find(child) != children_.end()) {
        path.shift();
        children_[child]->ls(path, out);
    } else {
        out << "child " << child << " not found!" << endl; // TODO
    }
}

void Object::print(const string &prefix)
{
    if (!children_.empty()) {
        std::cout << prefix << "Children:" << endl;
        for (auto it : children_) {
            it.second->print(prefix + "\t| ");
        }
        std::cout << prefix << endl;
    }
    if (!attribs_.empty()) {
        std::cout << prefix << "Attributes:" << endl;
        for (auto it : attribs_) {
            std::cout << prefix << "\t" << it.first
                      << " (" << it.second->typestr() << ")";
            std::cout << "\t[" << it.second->str() << "]";
            if (it.second->writeable()) {
                std::cout << "\tw";
            }
            if (!it.second->hookable()) {
                std::cout << "\t!h";
            }
            std::cout << endl;
        }
    }
    if (!actions_.empty()) {
        std::cout << prefix << "Actions:" << endl;
        std::cout << prefix;
        for (auto it : actions_) {
            std::cout << "\t" << it.first;
        }
        std::cout << endl;
    }
    std::cout << prefix << "Currently " << hooks_.size() << " hooks:" << endl;
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
    auto it_dyn = childrenDynamic_.find(name);
    if (it_dyn != childrenDynamic_.end()) {
        return it_dyn->second();
    }
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
        cur = cur->child(path.front());
        if (!cur) {
            output << "Object \"" << cur_path << "\""
                << " has no child named \"" << path.front()
                << "\"" << endl;
            return nullptr;
        }
        cur_path += path.front();
        cur_path += ".";
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

void Object::addDynamicChild(std::function<Object* ()> child, const std::string& name)
{
    childrenDynamic_[name] = child;
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

std::map<std::string, Object*> Object::children() {
    // copy the map of 'static' children
    auto allChildren = children_;
    // and add the dynamic children
    for (auto& it : childrenDynamic_) {
        Object* obj = it.second();
        if (obj) {
            allChildren[it.first] = obj;
        }
    }
    return allChildren;
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

