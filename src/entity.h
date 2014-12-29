#ifndef ENTITY
#define ENTITY

#include <string>
#include <memory>

namespace herbstluft {

enum class Type {
    VIRTUAL,
    SYMLINK,
    ACTION,
    ATTRIBUTE,
    ATTRIBUTE_INT,
    ATTRIBUTE_BOOL,
    ATTRIBUTE_COLOR,
    ATTRIBUTE_STRING,
    OBJECT,
    MONITOR,
    TAG,
    FRAME,
    CLIENT // TODO: etc. pp.
};

class Entity {
public:
    Entity() {}
    Entity(const std::string &name) : name_(name) {}
    virtual ~Entity() = 0;

    std::string name() { return name_; }
    virtual Type type() = 0;
    static std::string typestr(Type type);
    std::string typestr() { return typestr(type()); }

protected:
    std::string name_;
};

class Symlink : public Entity {
public:
    Symlink(const std::string name, std::shared_ptr<Entity> target)
     : Entity(name), target_(target) {}

    Type type() { return Type::SYMLINK; }
    std::shared_ptr<Entity> follow() { return target_; }

private:
    std::shared_ptr<Entity> target_;
};

}

#endif // ENTITY

