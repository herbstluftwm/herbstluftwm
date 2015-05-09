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
    DIRECTORY,
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
            "Generic", "Integer", "Boolean",
            "Color", "String",
            "Hook", "Directory", "Object",
            "Monitor", "Tag", "Frame", "Client"
        };
        return str[(int)type];
    }
    std::string typestr() { return typestr(type()); }

    static char typechar(Type type) {
        char const chr[] = {
            'v', 'l', '!',
            '?', 'i', 'b',
            'c', 's',
            'h', 'o', 'o',
            'o', 'o', 'o', 'o'
        };
        return chr[(int)type];
    }
    char typechar() { return typechar(type()); }

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

