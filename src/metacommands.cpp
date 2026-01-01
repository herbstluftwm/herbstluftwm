#include "metacommands.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <unordered_set>

#include "argparse.h"
#include "attribute_.h"
#include "command.h"
#include "completion.h"
#include "finite.h"
#include "ipc-protocol.h"
#include "regexstr.h"

using std::endl;
using std::function;
using std::pair;
using std::string;
using std::stringstream;
using std::shared_ptr;
using std::unique_ptr;
using std::vector;

extern char** environ;

/**
 * @brief The ObjectPointer class holds the pointer
 * to an Object and also it's path. Its main usage is
 * for the argument parsing of commands that accept an
 * object path as an argument.
 *
 * There is no parser directly for Object* because this would
 * make the calls to the parser to Monitor* ambiguous.
 */
class ObjectPointer {
public:
    string path_ = {}; //! the normalized object path without trailing '.'
    Object* object_ = {};
    static Object* root_; //! MetaCommands is responsible for setting this
};

Object* ObjectPointer::root_ = {};

template<> ObjectPointer Converter<ObjectPointer>::parse(const string& source) {
    if (ObjectPointer::root_) {
        ObjectPointer op;
        op.path_ = source;
        // normalize op.path_:
        while (!op.path_.empty() && *(op.path_.rbegin()) == OBJECT_PATH_SEPARATOR) {
            // while the last character is a dot, erase it:
            op.path_.erase(op.path_.size() - 1);
        }
        stringstream output;
        OutputChannels channels {"", output, output};
        Path path { op.path_, OBJECT_PATH_SEPARATOR };
        op.object_ = ObjectPointer::root_->child(path, channels);
        if (!op.object_) {
            string message = output.str();
            throw std::invalid_argument(message);
        }
        return op;
    } else {
        throw std::logic_error("ObjectPointer::root_ not initialized");
    }
}
template<> string Converter<ObjectPointer>::str(ObjectPointer payload)
{
    return payload.path_;
}
template<> void Converter<ObjectPointer>::complete(Completion& complete, ObjectPointer const*)
{

    if (ObjectPointer::root_) {
        MetaCommands::completeObjectPath(complete, ObjectPointer::root_);
    } else {
        throw std::logic_error("ObjectPointer::root_ not initialized");
    }
}


MetaCommands::MetaCommands(Object& root_) : root(root_) {
    ObjectPointer::root_ = &root;
}

int MetaCommands::get_attr_cmd(Input in, Output output) {
    string attrName;
    if (!(in >> attrName)) {
        return HERBST_NEED_MORE_ARGS;
    }

    Attribute* a = getAttribute(attrName, output);
    if (!a) {
        return HERBST_INVALID_ARGUMENT;
    }
    output << a->str();
    return 0;
}

int MetaCommands::set_attr_cmd(Input in, Output output) {
    string path, new_value;
    if (!(in >> path >> new_value)) {
        return HERBST_NEED_MORE_ARGS;
    }

    Attribute* a = getAttribute(path, output);
    if (!a) {
        return HERBST_INVALID_ARGUMENT;
    }
    string error_message = a->change(new_value);
    if (error_message.empty()) {
        return 0;
    } else {
        output.perror() << "\""
            << new_value << "\" is not a valid value for "
            << a->name() << ": "
            << error_message << endl;
        return HERBST_INVALID_ARGUMENT;
    }
}

int MetaCommands::attr_cmd(Input in, Output output) {
    string path = "", new_value = "";
    in >> path >> new_value;
    std::ostringstream dummy_output;
    Object* o = &root;
    auto p = Path::split(path);
    if (!p.empty()) {
        while (p.back().empty()) {
            p.pop_back();
        }
        o = o->child(p);
    }
    if (o && new_value.empty()) {
        o->ls(output);
        return 0;
    }

    Attribute* a = getAttribute(path, output);
    if (!a) {
        return HERBST_INVALID_ARGUMENT;
    }
    if (new_value.empty()) {
        // no more arguments -> return the value
        output << a->str();
        return 0;
    } else {
        // another argument -> set the value
        string error_message = a->change(new_value);
        if (error_message.empty()) {
            return 0;
        } else {
            output.perror() << "\""
                << new_value << "\" is not a valid value for "
                << a->name() << ": "
                << error_message << endl;
            return HERBST_INVALID_ARGUMENT;
        }
    }
}

Attribute* MetaCommands::getAttribute(string path, Output output) {
    try {
        return getAttributeOrException(path);
    } catch (const std::exception& exc) {
        output.perror() << exc.what() << endl;
        return nullptr;
    }
}

Attribute* MetaCommands::getAttributeOrException(string path)
{
    auto attr_path = Object::splitPath(path);
    auto child = root.child(attr_path.first);
    if (!child) {
        stringstream msg;
        msg << "No such object " << attr_path.first.join('.');
        throw std::invalid_argument(msg.str());
    }
    Attribute* a = child->attribute(attr_path.second);
    if (!a) {
        auto object_path = attr_path.first.join('.');
        if (object_path.empty()) {
            object_path = "The root object";
        } else {
            // equip object_path with quotes
            object_path = "Object \"" + object_path + "\"";
        }
        stringstream msg;
        msg << object_path
               << " has no attribute \"" << attr_path.second << "\"";
        throw std::invalid_argument(msg.str());
    }
    return a;

}

int MetaCommands::print_object_tree_command(Input in, Output output) {
    auto path = Path(in.empty() ? string("") : in.front()).toVector();
    while (!path.empty() && path.back().empty()) {
        path.pop_back();
    }
    auto child = root.child(path);
    if (!child) {
        output.perror() << "No such object " << Path(path).join('.') << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    child->printTree(output, Path(path).join('.'));
    return 0;
}

void MetaCommands::print_object_tree_complete(Completion& complete) {
    if (complete == 0) {
        completeObjectPath(complete);
    } else {
        complete.none();
    }
}

int MetaCommands::attrTypeCommand(Input input, Output output)
{
    string attrName;
    ArgParse ap;
    ap.mandatory(attrName);
    if (ap.parsingAllFails(input, output)) {
        return ap.exitCode();
    }

    Attribute* a = getAttribute(attrName, output);
    if (!a) {
        return HERBST_INVALID_ARGUMENT;
    }
    auto it = type_strings.find(a->type());
    string typeName = "unknown"; // this should not happen anyway
    if (it != type_strings.end()) {
        typeName = it->second.first;
    }
    output << typeName << endl;
    return 0;
}

void MetaCommands::attrTypeCompletion(Completion& complete)
{
    if (complete == 0) {
        completeAttributePath(complete);
    } else {
        complete.none();
    }
}


int MetaCommands::substitute_cmd(Input input, Output output)
{
    string ident, path;
    if (!(input >> ident >> path )) {
        return HERBST_NEED_MORE_ARGS;
    }
    Attribute* a = getAttribute(path, output);
    if (!a) {
        return HERBST_INVALID_ARGUMENT;
    }

    auto carryover = input.fromHere();
    carryover.replace(ident, a->str());
    return Commands::call(carryover, output);
}

void MetaCommands::substitute_complete(Completion& complete)
{
    if (complete == 0) {
        // no completion for the identifier
    } else if (complete == 1) {
        completeAttributePath(complete);
    } else {
        complete.completeCommands(2);
        if (!complete.noParameterExpected()) {
            // later, complete the identifier if the nested command
            // still expects parameters
            complete.full(complete[0]);
        }
    }
}

void MetaCommands::foreachCommand(CallOrComplete invoc)
{
    RegexStr filterName = {};
    string ident;
    bool unique = false;
    bool recursive = false;
    ObjectPointer object;
    ArgParse ap;
    ap.mandatory(ident).mandatory(object);
    ap.flags({
        {"--unique", &unique},
        {"--recursive", &recursive},
        {"--filter-name=", filterName},
    });
    ap.command(invoc,
               [&](Completion& complete) {
        // in the additional tokens, complete the identifier
        complete.full(ident);
        // and the command itself
        complete.completeCommands(0);
    },
               [&](ArgList command, Output output) -> int {
        if (command.empty()) {
            return  HERBST_NEED_MORE_ARGS;
        }
        Input cmd = { *(command.begin()), command.begin() + 1, command.end() };
        return foreachChild(ident, object.object_, object.path_,
                            unique, recursive, filterName, cmd, output);
    });
}

int MetaCommands::foreachChild(string ident,
                               Object* parent,
                               string pathString,
                               bool unique,
                               bool recursive,
                               const RegexStr& filterName,
                               Input nestedCommand,
                               Output output)
{
    // collect the paths of all children of this object
    vector<string> childPaths;
    // collect the children's names first to ensure that
    // object->children() is not changed by the commands we are
    // calling.
    if (recursive) {
        // if recursive, then one would also expect the root to be
        // traversed:
        childPaths.push_back(pathString);
    }
    if (!pathString.empty()) {
        pathString += OBJECT_PATH_SEPARATOR;
    }
    std::set<Object*> forbiddenObjects; // objects that may not be visited again
    std::queue<pair<string,Object*>> todoList; // objects that still need to be visited
    todoList.push(make_pair(pathString, parent));
    while (!todoList.empty()) {
        string objectPath = todoList.front().first;
        Object* object = todoList.front().second;
        todoList.pop();
        for (const auto& entry : object->children()) {
            Object* child = entry.second;
            if (!filterName.empty() && !filterName.matches(entry.first)) {
                // if we filter by name and the entry name does not match the filter,
                // then skip this child
                continue;
            }
            if (unique || recursive) {
                if (forbiddenObjects.find(child) != forbiddenObjects.end()) {
                    // if the object is already in the set of objects
                    // that may not be visited again, then
                    // skip this child;
                    continue;
                }
                forbiddenObjects.insert(child);
            }
            string currentChildPath = objectPath + entry.first;
            if (recursive) {
                todoList.push(make_pair(currentChildPath + OBJECT_PATH_SEPARATOR, child));
            }
            childPaths.push_back(currentChildPath);
        }
    }
    int  lastStatusCode = 0;
    for (const auto& child : childPaths) {
        Input carryover = nestedCommand;
        carryover.replace(ident, child);
        lastStatusCode = Commands::call(carryover, output);
    }
    return lastStatusCode;
}

//! parse a format string or throw an exception
MetaCommands::FormatString MetaCommands::parseFormatString(const string &format, size_t& idx)
{
    FormatString blobs;
    size_t lastpos = idx; // the position where the last plaintext blob started
    while (idx < format.size() && format[idx] != '}') {
        if (format[idx] == '%') {
            if (idx + 1 >= format.size()) {
                throw std::invalid_argument(
                    "dangling % at the end of format \"" + format + "\"");
            } else {
                if (idx > lastpos) {
                    // add literal text blob
                    blobs.push_back({ true, format.substr(lastpos, idx - lastpos), {}});
                }
                size_t format_type_idx = idx + 1;
                char format_type = format[format_type_idx];
                idx += 2;
                lastpos = idx;
                if (format_type == '%') {
                    blobs.push_back({true, "%", {}});
                } else if (format_type == 's') {
                    blobs.push_back({false, "s", {}});
                } else if (format_type == 'c') {
                    blobs.push_back({false, "c", {}});
                } else if (format_type == '{') {
                    FormatString nested = parseFormatString(format, idx);
                    blobs.push_back({false, "{", nested});
                    if (idx >= format.size() || format[idx] != '}') {
                        stringstream msg;
                        msg <<  "unmatched { at position "
                             << format_type_idx
                             << " in format \"" + format + "\"";
                        throw std::invalid_argument(msg.str());
                    }
                    idx++;
                    lastpos = idx;
                } else {
                    stringstream msg;
                    msg << "invalid format type %"
                        << format_type << " at position "
                        << format_type_idx << " in format string \""
                        << format << "\"";
                    throw std::invalid_argument(msg.str());
                }
            }
        } else {
            idx++;
        }
    }
    if (lastpos < idx) {
        blobs.push_back({true, format.substr(lastpos, idx - lastpos), {}});
    }
    return blobs;
}

MetaCommands::FormatString MetaCommands::parseFormatString(const string &format)
{
    size_t idx = 0;
    FormatString blobs;
    while (idx < format.size()) {
        if (format[idx] == '}') {
            // treat top level unmatched closing braces as literals
            blobs.push_back({true, "}", {}});
            idx++;
        } else {
            for (const auto& b : parseFormatString(format, idx)) {
                blobs.push_back(b);
            }
        }
    }
    return blobs;
}

string MetaCommands::evaluateFormatString(const FormatString& format,
                                          function<string()> nextToken,
                                          Output output)
{
    stringstream buf;
    for (auto& blob : format ) {
        if (blob.literal_) {
            buf << blob.data_;
        } else if (blob.data_ == "c") {
            buf << nextToken();
        } else if (blob.data_ == "{") {
            auto attrpath = evaluateFormatString(blob.nested_, nextToken, output);
            Attribute* a = getAttributeOrException(attrpath);
            if (a) {
                buf << a->str();
            }
        } else {
            // hence, data is "s", i.e. a %s format
            Attribute* a = getAttributeOrException(nextToken());
            if (a) {
                buf << a->str();
            }
        }
    }
    return buf.str();
}

int MetaCommands::sprintf_cmd(Input input, Output output)
{
    string ident, formatStringSrc;
    if (!(input >> ident >> formatStringSrc)) {
        return HERBST_NEED_MORE_ARGS;
    }
    FormatString format;
    string replacedString = "";
    function<string()> nextToken = [&input]() {
        string s;
        if (input >> s) {
            return s;
        } else {
            throw std::invalid_argument("not enough arguments");
        }
    };
    try {
        format = parseFormatString(formatStringSrc);
        // evaluate placeholders in the format string
        replacedString = evaluateFormatString(format, nextToken, output);
    }  catch (const std::invalid_argument& e) {
        output.perror() << e.what() << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    auto carryover = input.fromHere();
    carryover.replace(ident, replacedString);
    return Commands::call(carryover, output);
}

void MetaCommands::sprintf_complete(Completion& complete)
{
    if (complete == 0) {
        // no completion for arg name
    } else if (complete == 1) {
        // no completion for format string
    } else {
        FormatString fs;
        try {
            fs = parseFormatString(complete[1]);
        }  catch (const std::invalid_argument&) {
            complete.invalidArguments();
            return;
        }
        int indexOfNextArgument = 2;
        for (const auto& b : fs) {
            if (b.literal_ == true) {
                continue;
            }
            if (complete == indexOfNextArgument) {
                if (b.data_ == "s") {
                    completeAttributePath(complete);
                } else if (b.data_ == "c") {
                    // no completion for constant strings
                }
            }
            indexOfNextArgument++;
        }
        if (complete >= indexOfNextArgument) {
            complete.full(complete[0]); // complete string replacement
            complete.completeCommands(indexOfNextArgument);
        }
    }
}

static std::map<string, function<Attribute*(string)>> name2constructor {
    { "bool",  [](string n) { return new Attribute_<bool>(n, false); }},
    { "color", [](string n) { return new Attribute_<Color>(n, {"black"}); }},
    { "int",   [](string n) { return new Attribute_<int>(n, 0); }},
    { "string",[](string n) { return new Attribute_<string>(n, ""); }},
    { "uint",  [](string n) { return new Attribute_<unsigned long>(n, 0); }},
};

Attribute* MetaCommands::newAttributeWithType(string typestr, string attr_name, Output output) {
    auto it = name2constructor.find(typestr);
    if (it == name2constructor.end()) {
        output.perror() << "unknown type \"" << typestr << "\"";
        return nullptr;
    }
    auto attr = it->second(attr_name);
    attr->setWritable(true);
    return attr;
}

void MetaCommands::completeAttributeType(Completion& complete)
{
    for (const auto& t : name2constructor) {
        complete.full(t.first);
    }
}



int MetaCommands::new_attr_cmd(Input input, Output output)
{
    string type, path, initialValue;
    bool initialValueSupplied = false;
    ArgParse ap;
    ap.mandatory(type).mandatory(path);
    ap.optional(initialValue, &initialValueSupplied);
    if (ap.parsingFails(input, output)) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto obj_path_and_attr = Object::splitPath(path);
    string attr_name = obj_path_and_attr.second;
    Object* obj = root.child(obj_path_and_attr.first, output);
    if (!obj) {
        return HERBST_INVALID_ARGUMENT;
    }
    if (attr_name.substr(0,strlen(USER_ATTRIBUTE_PREFIX)) != USER_ATTRIBUTE_PREFIX) {
        output.perror()
            << "attribute name must start with \""
            << USER_ATTRIBUTE_PREFIX << "\""
            << " but is actually \"" << attr_name << "\"" << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    if (obj->attribute(attr_name)) {
        output.perror()
            << "object \"" << obj_path_and_attr.first.join()
            << "\" already has an attribute named \"" << attr_name
            <<  "\"" << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    // create the new attribute and add it
    Attribute* a = newAttributeWithType(type, attr_name, output);
    if (!a) {
        return HERBST_INVALID_ARGUMENT;
    }
    obj->addAttribute(a);
    userAttributes_.push_back(unique_ptr<Attribute>(a));
    // try to write the attribute
    if (initialValueSupplied) {
        string msg = a->change(initialValue);
        if (!msg.empty()) {
            output.perror() << "\""
                   << initialValue << "\" is an invalid "
                   << "value for " << path << ": " << msg << endl;
            return HERBST_INVALID_ARGUMENT;
        }
    }
    return 0;
}

void MetaCommands::new_attr_complete(Completion& complete)
{
    if (complete == 0) {
        completeAttributeType(complete);
    } else if (complete == 1) {
        completeObjectPath(complete, false);
        auto obj_path = Object::splitPath(complete[1]).first;
        if (obj_path.empty()) {
            complete.partial(USER_ATTRIBUTE_PREFIX);
        } else {
            char s[] = {OBJECT_PATH_SEPARATOR, '\0'};
            complete.partial(obj_path.join(OBJECT_PATH_SEPARATOR) + s + USER_ATTRIBUTE_PREFIX);
        }
    } else if (complete == 2) {
        // no completion for the initial value
    } else {
        complete.none();
    }
}

int MetaCommands::remove_attr_cmd(Input input, Output output)
{
    string path;
    if (!(input >> path )) {
        return HERBST_NEED_MORE_ARGS;
    }
    Attribute* a = root.deepAttribute(path, output);
    if (!a) {
        return HERBST_INVALID_ARGUMENT;
    }
    if (a->name().substr(0,strlen(USER_ATTRIBUTE_PREFIX)) != USER_ATTRIBUTE_PREFIX) {
        output.perror() << "Cannot remove built-in attribute \"" << path << "\"" << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    a->detachFromOwner();
    userAttributes_.erase(
        std::remove_if(
            userAttributes_.begin(),
            userAttributes_.end(),
            [a](const unique_ptr<Attribute>& p) { return p.get() == a; }),
        userAttributes_.end());
    return 0;
}

void MetaCommands::remove_attr_complete(Completion& complete) {
    if (complete == 0) {
        completeObjectPath(complete, true, [] (Attribute* a) {
            return a->name().substr(0,strlen(USER_ATTRIBUTE_PREFIX)) 
                    == USER_ATTRIBUTE_PREFIX;
        });
    } else {
        complete.none();
    }
}

template <typename T> int do_comparison(const T& a, const T& b) {
    return (a == b) ? 0 : 1;
}

template <> int do_comparison<int>(const int& a, const int& b) {
    if (a < b) {
        return -1;
    } else if (a > b) {
        return 1;
    } else {
        return 0;
    }
}
template <> int do_comparison<unsigned long>(const unsigned long& a, const unsigned long& b) {
    if (a < b) {
        return -1;
    } else if (a > b) {
        return 1;
    } else {
        return 0;
    }
}

template <typename T> int parse_and_compare(string a, string b, Output o) {
    vector<T> vals;
    for (auto &x : {a, b}) {
        try {
            vals.push_back(Converter<T>::parse(x));
        } catch(std::exception& e) {
            o.perror() << "cannot parse \"" << x << "\" to "
              << typeid(T).name() << ": " << e.what() << endl;
            return (int) HERBST_INVALID_ARGUMENT;
        }
    }
    return do_comparison<T>(vals[0], vals[1]);
}

//! operators of the 'compare' command
using CompareOperator = pair<bool, vector<int> >;

template <>
struct is_finite<CompareOperator> : std::true_type {};
template<> Finite<CompareOperator>::ValueList Finite<CompareOperator>::values = ValueListPlain {
    // map operator names to "for numeric types only" and possible return codes
    { { false, { 0 }, }, "=", },
    { { false, { -1, 1 } }, "!=", },
    { { true, { 1, 0 } }, "ge", },
    { { true, { 1 } }, "gt", },
    { { true, { -1, 0 } }, "le", },
    { { true, { -1 } }, "lt", },
};

int MetaCommands::compare_cmd(Input input, Output output)
{
    string path, value;
    CompareOperator oper;
    ArgParse ap = ArgParse().mandatory(path).mandatory(oper).mandatory(value);
    if (ap.parsingFails(input, output)) {
        return ap.exitCode();
    }
    Attribute* a = root.deepAttribute(path, output);
    if (!a) {
        return HERBST_INVALID_ARGUMENT;
    }
    // the following compare functions returns
    //   -1 if the first value is smaller
    //    1 if the first value is greater
    //    0 if the the values match
    //    HERBST_INVALID_ARGUMENT if there was a parsing error
    std::map<Type, pair<bool, function<int(string,string,Output)>>> type2compare {
        // map a type name to "is it numeric" and a comperator function
        { Type::INT,      { true,  parse_and_compare<int> }, },
        { Type::ULONG,    { true,  parse_and_compare<unsigned long> }, },
        { Type::BOOL,     { false, parse_and_compare<bool> }, },
        { Type::COLOR,    { false, parse_and_compare<Color> }, },
    };
    // the default comparison is simply string based:
    pair<bool, function<int(string,string,Output)>> comparator =
        { false, parse_and_compare<string> };
    auto it = type2compare.find(a->type());
    if (it != type2compare.end()) {
        comparator = it->second;
    }
    if (oper.first && !comparator.first) {
        output.perror()
            << "operator \"" << Converter<CompareOperator>::str(oper) << "\" "
            << "only allowed for numeric types, but the attribute "
            << path << " is of non-numeric type "
            << Entity::typestr(a->type()) << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    int comparison_result = comparator.second(a->str(), value, output);
    if (comparison_result > 1) {
        return comparison_result;
    }
    vector<int>& possible_values = oper.second;
    auto found = std::find(possible_values.begin(),
                           possible_values.end(),
                           comparison_result);
    return (found == possible_values.end()) ? 1 : 0;
}

void MetaCommands::compare_complete(Completion &complete) {
    if (complete == 0) {
        completeAttributePath(complete);
    } else if (complete == 1) {
        Converter<CompareOperator>::complete(complete);
    } else if (complete == 2) {
        // no completion suggestions for the 'value' field
    } else {
        complete.none();
    }
}


void MetaCommands::completeObjectPath(Completion& complete, Object* rootObject,
                                      bool attributes,
                                      function<bool(Attribute*)> attributeFilter)
{
    ArgList objectPathArgs = std::get<0>(Object::splitPath(complete.needle()));
    string objectPath = objectPathArgs.join(OBJECT_PATH_SEPARATOR);
    if (!objectPath.empty()) {
        objectPath += OBJECT_PATH_SEPARATOR;
    }
    Object* object = rootObject->child(objectPathArgs);
    if (!object) {
        return;
    }
    if (attributes) {
        for (auto& it : object->attributes()) {
            if (attributeFilter && !attributeFilter(it.second)) {
                continue;
            }
            complete.full(objectPath + it.first);
        }
    }
    for (auto& it : object->children()) {
        complete.partial(objectPath + it.first + OBJECT_PATH_SEPARATOR);
    }
}

void MetaCommands::completeObjectPath(Completion& complete, bool attributes,
                                      function<bool(Attribute*)> attributeFilter)
{
    MetaCommands::completeObjectPath(complete, &root, attributes, attributeFilter);
}

void MetaCommands::completeAttributePath(Completion& complete) {
    completeObjectPath(complete, true);
}

int MetaCommands::helpCommand(Input input, Output output)
{
    string needle;
    ArgParse args = ArgParse().mandatory(needle);
    if (args.parsingAllFails(input, output)) {
        return args.exitCode();
    }
    function<string(string)> header =
            [] (const string& text) {
        string buf = text + "\n";
        for (size_t i = 0; i < text.size(); i++) {
            buf += "-";
        }
        buf += "\n";
        return buf;
    };
    // drop trailing '.'
    if (!needle.empty() && *needle.rbegin() == '.') {
        needle.pop_back();
    }
    // split the needle into the path to the owning object
    // and the name of the entry.
    auto path = Object::splitPath(needle);
    Object* parent = root.child(path.first, output);
    const HasDocumentation* childDoc = nullptr;
    Object* object = nullptr;
    if (parent) {
        object = parent->child(path.second);
    }
    if (needle.empty()) {
        object = &root;
    }
    bool helpFound = false;
    if (parent) {
        Attribute* attribute = parent->attribute(path.second);
        if (attribute) {
            helpFound = true;
            output << header("Attribute \'" + path.second + "\' of \'"
                             + path.first.join() + "\'");
            output << endl; // empty line
            output << "Type: " << Entity::typestr(attribute->type()) << endl;
            output << "Current value: " << attribute->str() << endl;
            output << endl; // empty line
            const string& doc = attribute->doc();
            if (!doc.empty()) {
                output << doc;
                output << endl;
                output << endl; // empty line
            }
        }
        // only print documentation on the entry if there
        // is anything particular to be said.
        childDoc = parent->childDoc(path.second);
        if (childDoc && !childDoc->doc().empty()) {
            helpFound = true;
            output << header("Entry \'" + path.second + "\' of \'"
                             + path.first.join() + "\'");
            output << childDoc->doc() << endl;
            output << endl; // empty line
        }
    }
    if (object || childDoc) {
        helpFound = true;
        output << header("Object \'" + needle + "\'");
    }
    if (childDoc && !object) {
        // if the entry may exist at some time but not at the
        // moment then make the user aware of this
        output << "(Entry does not exist at the moment)" << endl;
    } else if (object) {
        object->ls(output);
    }
    if (!helpFound) {
        output.perror() << "No help found for \'" << needle << "\'" << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    return 0;
}

void MetaCommands::helpCompletion(Completion& complete)
{
    if (complete == 0) {
        completeAttributePath(complete);
    } else {
        complete.none();
    }
}

void MetaCommands::get_attr_complete(Completion& complete) {
    if (complete == 0) {
        completeAttributePath(complete);
    } else {
        complete.none();
    }
}

void MetaCommands::set_attr_complete(Completion& complete) {
    if (complete == 0) {
        completeObjectPath(complete, true,
            [](Attribute* a) { return a->writable(); } );
    } else if (complete == 1) {
        Attribute* a = root.deepAttribute(complete[0]);
        if (a) {
            a->complete(complete);
        }
    } else {
        complete.none();
    }
}

void MetaCommands::attr_complete(Completion& complete)
{
    if (complete == 0) {
        completeAttributePath(complete);
    } else if (complete == 1) {
        Attribute* a = root.deepAttribute(complete[0]);
        if (!a) {
            return;
        }
        if (a->writable()) {
            a->complete(complete);
        } else {
            complete.none();
        }
    } else {
        complete.none();
    }
}

int MetaCommands::tryCommand(Input input, Output output) {
    Commands::call(input.fromHere(), output); // pass output
    return 0; // ignore exit code
}

int MetaCommands::silentCommand(Input input, Output output) {
    stringstream dummyOutput;
    // discard output and error channel
    // TODO: pass through the error channel as soon as
    // the ipc-protocol supports it.
    OutputChannels discardOutputChannels(output.command(), dummyOutput, dummyOutput);
    // drop output but pass exit code
    return Commands::call(input.fromHere(), discardOutputChannels);
}

int MetaCommands::negateCommand(Input input, Output output)
{
    return ! Commands::call(input.fromHere(), output);
}

void MetaCommands::completeCommandShifted1(Completion& complete) {
    complete.completeCommands(0);
}

int MetaCommands::echoCommand(Input input, Output output)
{
    string token;
    bool first = true;
    while (input >> token) {
        output << (first ? "" : " ") << token;
        first = false;
    }
    output << endl;
    return 0;
}


int MetaCommands::setenvCommand(Input input, Output output) {
    string name, value;
    if (!(input >> name >> value)) {
        return HERBST_NEED_MORE_ARGS;
    }
    if (setenv(name.c_str(), value.c_str(), 1) != 0) {
        output.perror()
               << "Could not set environment variable: "
               << strerror(errno) << endl;
        return HERBST_UNKNOWN_ERROR;
    }
    return 0;
}

void MetaCommands::setenvCompletion(Completion& complete) {
    if (complete == 0) {
        return completeEnvName(complete);
    } else if (complete == 1) {
        // no completion for the value
    } else {
        complete.none();
    }
}

//! a wraper around setenv with the usual 'export' syntax in posix
int MetaCommands::exportEnvCommand(Input input, Output output)
{
    string arg;
    if (!(input >> arg )) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto pos = arg.find("=");
    if (pos >= arg.size()) {
        return HERBST_NEED_MORE_ARGS;
    }
    // if "=" has been found in the arg, split it there
    auto newInput = Input(input.command(), {arg.substr(0, pos), arg.substr(pos + 1)});
    return setenvCommand(newInput, output);
}

void MetaCommands::exportEnvCompletion(Completion &complete)
{
    for (char** env = environ; *env; ++env) {
        vector<string> chunks = ArgList::split(*env, '=');
        if (!chunks.empty()) {
            complete.partial(chunks[0] + "=");
        }
    }
}

int MetaCommands::getenvCommand(Input input, Output output) {
    string name;
    if (!(input >> name)) {
        return HERBST_NEED_MORE_ARGS;
    }
    char* envvar = getenv(name.c_str());
    if (!envvar) {
        return HERBST_ENV_UNSET;
    }
    output << envvar << endl;
    return 0;
}

//! completion for unsetenv and getenv
void MetaCommands::getenvUnsetenvCompletion(Completion& complete) {
    if (complete == 0) {
        return completeEnvName(complete);
    } else {
        complete.none();
    }
}

int MetaCommands::unsetenvCommand(Input input, Output output) {
    string name;
    if (!(input >> name)) {
        return HERBST_NEED_MORE_ARGS;
    }
    if (unsetenv(name.c_str()) != 0) {
        output.perror()
               << "Could not unset environment variable: "
               << strerror(errno) << endl;
        return HERBST_UNKNOWN_ERROR;
    }
    return 0;
}

void MetaCommands::completeEnvName(Completion& complete) {
    for (char** env = environ; *env; ++env) {
        vector<string> chunks = ArgList::split(*env, '=');
        if (!chunks.empty()) {
            complete.full(chunks[0]);
        }
    }
}

int MetaCommands::chainCommand(Input input, Output output)
{
    vector<vector<string>> commands = splitCommandList(input.toVector());
    int returnCode = 0;
    // the condition that has to be fulfilled if we want to continue
    // execuding commands. the default (for 'chain') is to always continue
    function<bool(int)> conditionContinue = [](int) { return true; };
    if (input.command() == "and") {
        // continue executing commands while they are successful
        conditionContinue = [](int code) { return code == 0; };
    }
    if (input.command() == "or") {
        returnCode = 1;
        // continue executing commands while they are failing
        conditionContinue = [](int code) { return code >= 1; };
    }
    for (auto& cmd : commands) {
        if (cmd.empty()) {
            // if command range is empty, do nothing
            continue;
        }
        Input cmdinput = Input(cmd[0], cmd.begin() + 1, cmd.end());
        returnCode = Commands::call(cmdinput, output);
        if (!conditionContinue(returnCode)) {
            break;
        }
    }
    return returnCode;
}

void MetaCommands::chainCompletion(Completion& complete)
{
    if (complete == 0) {
        // no completion for the separator
    } else {
        size_t lastsep = 0;
        for (size_t i = 1; i < complete.needleIndex(); i++) {
            if (complete[i] == complete[0]) {
                lastsep = i;
            }
        }
        if (complete > lastsep + 1) {
            complete.full(complete[0]);
        }
        complete.completeCommands(lastsep + 1);
        // even if the completion of the parameter
        // claimed that no parameters are expected, there still
        // could be another separator and another command
        complete.parametersStillExpected();
    }
}


vector<vector<string>> MetaCommands::splitCommandList(ArgList::Container input) {
    vector<vector<string>> res;
    if (input.empty()) {
        return res;
    }
    vector<string> current;
    string separator = input[0];
    for (size_t i = 1; i < input.size(); i++) {
        if (input[i] == separator) {
            res.push_back(current);
            current = {};
        } else {
            current.push_back(input[i]);
        }
    }
    res.push_back(current);
    return res;
}

