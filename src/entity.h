#ifndef ENTITY
#define ENTITY

#include <map>
#include <string>

enum class Type {
    INT,
    ULONG,
    BOOL,
    DECIMAL,
    COLOR,
    STRING,
    REGEX,
    NAMES, // a enum type containing names
    FONT,
    RECTANGLE,
    WINDOW,
};

static const std::map<Type, std::pair<std::string, char>> type_strings = {
    {Type::BOOL,    {"bool",         'b'}},
    {Type::COLOR,   {"color",        'c'}},
    {Type::DECIMAL, {"decimal",      'd'}},
    {Type::FONT,    {"font",         'f'}},
    {Type::INT,     {"int",          'i'}},
    {Type::NAMES,   {"names",        'n'}},
    {Type::RECTANGLE, {"rectangle",  'R'}},
    {Type::REGEX,   {"regex",        'r'}},
    {Type::STRING,  {"string",       's'}},
    {Type::ULONG,   {"uint",         'u'}},
    {Type::WINDOW,  {"windowid",     'w'}},
};

bool operator<(Type t1, Type t2);

class HasDocumentation  {
public:
    void setDoc(const char text[]) { doc_ = text; }
    std::string doc() const { return doc_ ? doc_ : ""; };
private:
    /** we avoid the duplication of the doc string
     * with every instance of the owning class.
     */
    const char* doc_ = nullptr;
};

class Entity {
public:
    Entity() = default;
    Entity(const std::string &name) : name_(name) {}
    virtual ~Entity() = default;

    std::string name() const { return name_; }
    virtual Type type() = 0;
    static std::string typestr(Type type) {
        return type_strings.at(type).first;
    }
    std::string typestr() { return typestr(type()); }

    static char typechar(Type type) {
        return type_strings.at(type).second;
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


#endif // ENTITY

