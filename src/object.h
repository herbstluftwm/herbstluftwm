/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HS_OBJECT_H_
#define __HS_OBJECT_H_

#include "types.h"
#include "hook.h"

#include <map>
#include <vector>
#include <functional>


#define OBJECT_PATH_SEPARATOR '.'
#define USER_ATTRIBUTE_PREFIX "my_"
#define TMP_OBJECT_PATH "tmp"

class Attribute;
class Action;

enum class HookEvent {
    CHILD_ADDED,
    CHILD_REMOVED,
    ATTRIBUTE_CHANGED
};

class Object {

public:
    Object();
    virtual ~Object() {}

    virtual void print(const std::string &prefix = "\t| "); // a debug method

    // object tree ls command
    virtual void ls(Output out);
    virtual void ls(Path path, Output out); // traversial version

    virtual std::string read(const std::string &attr) const;
    virtual bool writeable(const std::string &attr) const;
    virtual void write(const std::string &attr, const std::string &value);
    virtual bool hookable(const std::string &attr) const;

    virtual void trigger(const std::string &action, ArgList args);

    static std::pair<ArgList,std::string> splitPath(const std::string &path);

    // return an attribute if it exists, else NULL
    Attribute* attribute(const std::string &name);

    // return an attribute by parsing the path and possibly looking at children
    Attribute* deepAttribute(const std::string &path);
    Attribute* deepAttribute(const std::string &path, Output output);

    void addAttribute(Attribute* a);
    void removeAttribute(Attribute* a);
    std::map<std::string, Attribute*> attributes() { return attribs_; }

    std::string acceptAllValueValidator() { return {}; };
    static std::function<std::string()> AcceptAll() {
        return []() { return ""; };
    };

    // if a concrete object maintains its index within the parent as an
    // attribute (e.g. monitors and tags do), then they should implement the
    // following, such that the parent can tell the child its index.
    virtual void setIndexAttribute(unsigned long index) { };

    Object* child(const std::string &name);

    Object* child(Path path);

    Object* child(Path path, Output output);

    /* Called by the directory whenever children are added or removed */
    void notifyHooks(HookEvent event, const std::string &arg);

    void addChild(Object* child, const std::string &name);
    void addStaticChild(Object* child, const std::string &name);
    void removeChild(const std::string &child);

    void addHook(Hook* hook);
    void removeHook(Hook* hook);

    const std::map<std::string, Object*>& children() { return children_; }

    void printTree(Output output, std::string rootLabel);

protected:
    // initialize an attribute (typically used by init())
    virtual void wireAttributes(std::vector<Attribute*> attrs);
    virtual void wireActions(std::vector<Action*> actions);

    std::map<std::string, Attribute*> attribs_;
    std::map<std::string, Action*> actions_;

    std::map<std::string, Object*> children_;
    std::vector<Hook*> hooks_;

    //DynamicAttribute nameAttribute_;
};


#endif

