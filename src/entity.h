#ifndef ENTITY
#define ENTITY

#include <map>
#include <string>

enum class Type {
    VIRTUAL,
    SYMLINK,
    ACTION,
    ATTRIBUTE,
    ATTRIBUTE_INT,
    ATTRIBUTE_ULONG,
    ATTRIBUTE_BOOL,
    ATTRIBUTE_DECIMAL,
    ATTRIBUTE_COLOR,
    ATTRIBUTE_STRING,
    ATTRIBUTE_REGEX,
    ATTRIBUTE_NAMES, // a enum type containing names
    ATTRIBUTE_FONT,
    HOOK,
    DIRECTORY,
    OBJECT,
    MONITOR,
    TAG,
    FRAME,
    CLIENT // TODO: etc. pp.
};

static const std::map<Type, std::pair<std::string, char>> type_strings = {
    {Type::VIRTUAL,           {"Virtual Node", 'v'}},
    {Type::SYMLINK,           {"Symbolic Link",'l'}},
    {Type::ACTION,            {"Action",       '!'}},
    {Type::ATTRIBUTE,         {"Generic",      '?'}},
    {Type::ATTRIBUTE_INT,     {"Integer",      'i'}},
    {Type::ATTRIBUTE_ULONG,   {"Unsigned",     'u'}},
    {Type::ATTRIBUTE_BOOL,    {"Boolean",      'b'}},
    {Type::ATTRIBUTE_DECIMAL, {"Decimal",      'd'}},
    {Type::ATTRIBUTE_COLOR,   {"Color",        'c'}},
    {Type::ATTRIBUTE_STRING,  {"String",       's'}},
    {Type::ATTRIBUTE_REGEX,   {"Regex",        'r'}},
    {Type::ATTRIBUTE_NAMES,   {"Names",        'n'}},
    {Type::ATTRIBUTE_FONT,    {"Font",         'f'}},
    {Type::HOOK,              {"Hook",         'h'}},
    {Type::DIRECTORY,         {"Directory",    'o'}},
    {Type::OBJECT,            {"Object",       'o'}},
    {Type::MONITOR,           {"Monitor",      'o'}},
    {Type::TAG,               {"Tag",          'o'}},
    {Type::FRAME,             {"Frame",        'o'}},
    {Type::CLIENT,            {"Client",       'o'}},
};

bool operator<(Type t1, Type t2);

class HasDocumentation  {
public:
    void setDoc(const char text[]) { doc_ = text; }
    std::string doc() const { return doc_; };
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

