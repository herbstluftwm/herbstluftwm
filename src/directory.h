#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "entity.h"
#include "utils.h" // for Output and stuff object.h needs

#include <memory>
#include <map>

namespace herbstluft {

class Hook;

enum class HookEvent {
    CHILD_ADDED,
    CHILD_REMOVED,
    ATTRIBUTE_CHANGED
};

class Directory : public Entity, public std::enable_shared_from_this<Directory>
{
public:
    Directory(const std::string& name) : Entity(name) {}
    virtual ~Directory() {};

    virtual Type type() { return Type::DIRECTORY; }

    virtual void print(const std::string &prefix = "\t| "); // a debug method

    // object tree ls command
    virtual void ls(Output out); // local version
    virtual void ls(Path path, Output out); // traversial version

    virtual bool exists(const std::string &name) {
        return children_.find(name) != children_.end();
    }

    const std::map<std::string, std::shared_ptr<Directory>>&
    children() { return children_; }

    template<typename T>
    std::shared_ptr<T> child(const std::string &name) {
        auto it = children_.find(name);
        if (it != children_.end())
            return it->second->ptr<T>();
        else
            return {};
    }
    template<typename T>
    std::shared_ptr<T> child(Path path) {
        if (path.empty()) {
            return ptr<T>();
        }
        auto it = children_.find(path.front());
        if (it != children_.end())
            return it->second->child<T>(path + 1);
        else
            return {};
    }

    /* Called by the directory whenever children are added or removed */
    void notifyHooks(HookEvent event, const std::string &arg);

    void addChild(std::shared_ptr<Directory> child, std::string name = {});
    void addStaticChild(Directory *child, std::string name = {});
    void removeChild(const std::string &child);

    void addHook(std::shared_ptr<Hook> hook);
    void removeHook(const std::string &hook);

    void printTree(Output output);

protected:
    /* convenience function to be used by objects to return themselves. */
    template<typename T>
    std::shared_ptr<T> ptr() {
        return std::dynamic_pointer_cast<T>(shared_from_this());
    }

    std::map<std::string, std::shared_ptr<Directory>> children_;
    std::map<std::string, std::weak_ptr<Hook>> hooks_;
};

}

#endif // DIRECTORY_H
