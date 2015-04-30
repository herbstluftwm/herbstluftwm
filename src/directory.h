#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "entity.h"
#include "x11-types.h" // for Output and stuff object.h needs

#include <memory>
#include <unordered_map>

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
    virtual void ls(Output out);

    virtual bool exists(const std::string &name) {
        return children_.find(name) != children_.end();
    }

    const std::unordered_map<std::string, std::shared_ptr<Directory>>&
    children() { return children_; }

    template<typename T>
    std::shared_ptr<T> child(const std::string &name) {
        auto it = children_.find(name);
        if (it != children_.end())
            return it->second->ptr<T>();
        else
            return {};
    }

    /* Called by the directory whenever children are added or removed */
    void notifyHooks(HookEvent event, const std::string &arg);

    void addChild(std::shared_ptr<Directory> child);
    void removeChild(const std::string &child);

    void addHook(std::shared_ptr<Hook> hook);
    void removeHook(const std::string &hook);

protected:
    /* convenience function to be used by objects to return themselves. */
    template<typename T>
    std::shared_ptr<T> ptr() {
        return std::dynamic_pointer_cast<T>(shared_from_this());
    }

    std::unordered_map<std::string, std::shared_ptr<Directory>> children_;
    std::unordered_map<std::string, std::weak_ptr<Hook>> hooks_;
};

}

#endif // DIRECTORY_H
