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
    {Type::INT,     {"Integer",      'i'}},
    {Type::ULONG,   {"Unsigned",     'u'}},
    {Type::BOOL,    {"Boolean",      'b'}},
    {Type::DECIMAL, {"Decimal",      'd'}},
    {Type::COLOR,   {"Color",        'c'}},
    {Type::STRING,  {"String",       's'}},
    {Type::REGEX,   {"Regex",        'r'}},
    {Type::NAMES,   {"Names",        'n'}},
    {Type::FONT,    {"Font",         'f'}},
    {Type::RECTANGLE, {"Rectangle",  'R'}},
    {Type::WINDOW,  {"WindowID",     'w'}},
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

