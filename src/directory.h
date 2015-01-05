#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "entity.h"
#include "hook.h"

#include <memory>
#include <unordered_map>

namespace herbstluft {

class Hook;

class Directory : public Entity, public std::enable_shared_from_this<Directory>
{
public:
    Directory(const std::string& name) : Entity(name) {}

    virtual Type type() { return Type::DIRECTORY; }

    virtual void print(const std::string &prefix = "\t| "); // a debug method

    const std::unordered_map<std::string, std::shared_ptr<Directory>>&
    children() { return children_; }

    /* Called by the directory whenever children are added or removed */
    void notifyHooks(Hook::Event event, const std::string &arg);

    void addChild(std::shared_ptr<Directory> child);
    void removeChild(const std::string &child);

    void addHook(std::shared_ptr<Hook> hook);
    void removeHook(const std::string &hook);

protected:
    std::unordered_map<std::string, std::shared_ptr<Directory>> children_;
    std::unordered_map<std::string, std::weak_ptr<Hook>> hooks_;
};

}

#endif // DIRECTORY_H
