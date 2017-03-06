/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HS_OBJECT_H_
#define __HS_OBJECT_H_

#include "utils.h" // for Output and stuff object.h needs
#include "attribute.h"
#include "hook.h"

#include <map>
#include <memory>
#include <vector>
#include <functional>


#define OBJECT_PATH_SEPARATOR '.'
#define USER_ATTRIBUTE_PREFIX "my_"
#define TMP_OBJECT_PATH "tmp"
#define ACCEPT_ALL (AcceptAll())

enum class HookEvent {
    CHILD_ADDED,
    CHILD_REMOVED,
    ATTRIBUTE_CHANGED
};

class Object : public std::enable_shared_from_this<Object> {

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

    std::map<std::string, Attribute*> attributes() { return attribs_; }

    std::string acceptAllValueValidator() { return {}; };
    static std::function<std::string()> AcceptAll() {
        return []() { return ""; };
    };

    // if a concrete object maintains its index within the parent as an
    // attribute (e.g. monitors and tags do), then they should implement the
    // following, such that the parent can tell the child its index.
    virtual void setIndexAttribute(unsigned long index) { };

    std::shared_ptr<Object> child(const std::string &name);

    std::shared_ptr<Object> child(Path path);

    /* Called by the directory whenever children are added or removed */
    void notifyHooks(HookEvent event, const std::string &arg);

    void addChild(std::shared_ptr<Object> child, std::string name);
    void addStaticChild(Object *child, std::string name);
    void removeChild(const std::string &child);

    void addHook(Hook* hook);
    void removeHook(Hook* hook);

    const std::map<std::string, std::shared_ptr<Object>>&
    children() { return children_; }

    void printTree(Output output, std::string rootLabel);

protected:
    // initialize an attribute (typically used by init())
    virtual void wireAttributes(std::vector<Attribute*> attrs);
    virtual void wireActions(std::vector<Action*> actions);

    std::map<std::string, Attribute*> attribs_;
    std::map<std::string, Action*> actions_;

    /* convenience function to be used by objects to return themselves. */
    template<typename T>
    std::shared_ptr<T> ptr() {
        return std::dynamic_pointer_cast<T>(shared_from_this());
    }

    std::map<std::string, std::shared_ptr<Object>> children_;
    std::vector<Hook*> hooks_;

    //DynamicAttribute nameAttribute_;
};


#endif

