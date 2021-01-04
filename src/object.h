#ifndef __HS_OBJECT_H_
#define __HS_OBJECT_H_

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "entity.h"
#include "types.h"

#define OBJECT_PATH_SEPARATOR '.'
#define USER_ATTRIBUTE_PREFIX "my_"
#define TMP_OBJECT_PATH "tmp"

class Attribute;
class Action;
class Hook;
class Object;

/*! a child object entry of an object is like the subdirectory of
 * a directory. In addition, we have documentation, what this entry
 * is good for.
 */
class ChildEntry : public HasDocumentation {
protected:
    ChildEntry(Object& owner, const std::string& name)
        : owner_(owner)
        , name_(name)
    {}
    Object& owner_;
    std::string name_;
};

enum class HookEvent {
    CHILD_ADDED,
    CHILD_REMOVED,
    ATTRIBUTE_CHANGED
};

class Object : public HasDocumentation {

public:
    Object() = default;
    virtual ~Object() = default;

    // object tree ls command
    virtual void ls(Output out);

    static std::pair<ArgList,std::string> splitPath(const std::string &path);

    // return an attribute if it exists, else NULL
    Attribute* attribute(const std::string &name);

    // return an attribute by parsing the path and possibly looking at children
    Attribute* deepAttribute(const std::string &path);
    Attribute* deepAttribute(const std::string &path, Output output);

    void addAttribute(Attribute* a);
    void removeAttribute(Attribute* a);
    std::map<std::string, Attribute*> attributes() { return attribs_; }

    // if a concrete object maintains its index within the parent as an
    // attribute (e.g. monitors and tags do), then they should implement the
    // following, such that the parent can tell the child its index.
    virtual void setIndexAttribute(unsigned long index) { };

    Object* child(const std::string &name);

    Object* child(Path path);

    Object* child(Path path, Output output);

    /* Called by the directory whenever children are added or removed */
    void notifyHooks(HookEvent event, const std::string &arg);

    /** a child with the given name exists if the function
     * returns a non-null pointer
     */
    void addDynamicChild(std::function<Object*()> child, const std::string &name);

    void addChild(Object* child, const std::string &name);
    void removeChild(const std::string &child);

    void addHook(Hook* hook);
    void removeHook(Hook* hook);

    std::map<std::string, Object*> children();

    void printTree(Output output, std::string rootLabel);

protected:
    // initialize an attribute (typically used by init())
    virtual void wireAttributes(std::vector<Attribute*> attrs);
    virtual void wireActions(std::vector<Action*> actions);

    std::map<std::string, Attribute*> attribs_;
    std::map<std::string, Action*> actions_;

    std::map<std::string, std::function<Object*()>> childrenDynamic_;
    std::map<std::string, Object*> children_;
    std::vector<Hook*> hooks_;

    //DynamicAttribute nameAttribute_;
};


#endif

