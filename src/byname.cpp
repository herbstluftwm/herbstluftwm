#include "byname.h"

#include <cassert>
#include <utility>

#include "attribute.h"

using std::string;

ByName::ByName(Object& parent_)
    : parent(parent_)
{
    parent_.addChild(this, "by-name");
    parent_.addHook(this);
    setDoc(
        "This has an entry \'name\' for every object with "
        "the given \'name\'. If an object has an empty name "
        "then it is not listed here."
    );
}

ByName::~ByName() {
    parent.removeHook(this);
}

void ByName::childAdded(Object* sender_parent, string child_name)
{
    if (&* sender_parent != &parent) {
        return;
    }
    // child_name is the value how sender_parent calls the child
    Object* child = parent.child(child_name);
    Attribute* name_attrib = child->attribute("name");
    // if the new child has a name attribute, then we list it as well
    if (name_attrib) {
        last_name[child] = name_attrib->str();
        child->addHook(this);
        if (!name_attrib->str().empty()) {
            // we only list it, if the name is non-empty
            addChild(child, name_attrib->str());
        }
    }
}

void ByName::childRemoved(Object* sender_parent, string child_name)
{
    if (&* sender_parent != &parent) {
        return;
    }
    // child_name is the value how sender_parent calls the child
    Object* child = parent.child(child_name);
    auto it = last_name.find(child);
    // if we cared about this child previously
    if (it != last_name.end()) {
        child->removeHook(this);
        assert(it != last_name.end());
        removeChild(it->second);
        last_name.erase(it);
    }
}

void ByName::attributeChanged(Object* child, string attribute_name)
{
    //std::cerr << "Attribute " << attribute_name << " changed" << endl;
    if (attribute_name != "name" || &* child == &parent) {
        return;
    }
    auto it = last_name.find(child);
    Attribute* name_attrib = child->attribute("name");
    if (it == last_name.end() || !name_attrib) {
        // we are not monitoring it.
        // this can only happen if child == parent
        return;
    }
    auto new_name = name_attrib->str();
    if (!it->second.empty()) {
        removeChild(it->second);
    }
    last_name[child] = new_name;
    if (!new_name.empty()) {
        addChild(child, new_name);
    }
}

