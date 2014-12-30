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
    HOOK,
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
    virtual ~Entity() {};

    std::string name() { return name_; }
    virtual Type type() = 0;
    static std::string typestr(Type type) {
        const char * const str[] = {
            "Virtual Node", "Symbolic Link", "Action",
            "Attribute", "Attribute(int)", "Attribute(bool)",
            "Attribute(color)", "Attribute(string)",
            "Hook", "Object",
            "Monitor", "Tag", "Frame", "Client"
        };
        return str[(int)type];
    }

    std::string typestr() { return typestr(type()); }

protected:
    std::string name_;
};

/* // not yet supported
class Symlink : public Entity {
public:
    Symlink(const std::string name, std::shared_ptr<Entity> target)
     : Entity(name), target_(target) {}

    Type type() { return Type::SYMLINK; }
    std::shared_ptr<Entity> follow() { return target_; }

private:
    std::shared_ptr<Entity> target_;
};
*/

}

#endif // ENTITY

