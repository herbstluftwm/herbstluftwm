#include "metacommands.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>

#include "argparse.h"
#include "attribute_.h"
#include "command.h"
#include "completion.h"
#include "finite.h"
#include "ipc-protocol.h"

using std::endl;
using std::function;
using std::pair;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;

extern char** environ;

MetaCommands::MetaCommands(Object& root_) : root(root_) {
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
        output << in.command() << ": \""
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
            output << in.command() << ": \""
                << new_value << "\" is not a valid value for "
                << a->name() << ": "
                << error_message << endl;
            return HERBST_INVALID_ARGUMENT;
        }
    }
}

Attribute* MetaCommands::getAttribute(string path, Output output) {
    auto attr_path = Object::splitPath(path);
    auto child = root.child(attr_path.first);
    if (!child) {
        output << "No such object " << attr_path.first.join('.') << endl;
        return nullptr;
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
        output << object_path
               << " has no attribute \"" << attr_path.second << "\""
               << endl;
        return nullptr;
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
        output << "No such object " << Path(path).join('.') << endl;
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
        // later, complete the identifier
        complete.full(complete[0]);
        complete.completeCommands(2);
    }
}

int MetaCommands::foreachCmd(Input input, Output output)
{
    string ident, pathString;
    if (!(input >> ident >> pathString)) {
        return HERBST_NEED_MORE_ARGS;
    }
    // remove trailing dots to avoid parsing issues:
    while (!pathString.empty() && pathString.back() == OBJECT_PATH_SEPARATOR) {
        // while the last character is a dot, erase it:
        pathString.erase(pathString.size() - 1);
    }
    Path path { pathString, OBJECT_PATH_SEPARATOR };
    Object* object = root.child(path, output);
    if (!object) {
        return HERBST_INVALID_ARGUMENT;
    }

    // collect the paths of all children of this object
    vector<string> childPaths;
    // collect the children's names first to ensure that
    // object->children() is not changed by the commands we are
    // calling.
    if (!pathString.empty()) {
        pathString += OBJECT_PATH_SEPARATOR;
    }
    for (const auto& entry : object->children()) {
        childPaths.push_back(pathString + entry.first);
    }
    int  lastStatusCode = 0;
    for (const auto& child : childPaths) {
        Input carryover = input.fromHere();
        carryover.replace(ident, child);
        lastStatusCode = Commands::call(carryover, output);
    }
    return lastStatusCode;
}

void MetaCommands::foreachComplete(Completion& complete)
{
    if (complete == 0) {
        // no completion for the identifier
    } else if (complete == 1) {
        completeObjectPath(complete);
    } else {
        // later, complete the identifier
        complete.full(complete[0]);
        complete.completeCommands(2);
    }
}

//! parse a format string or throw an exception
MetaCommands::FormatString MetaCommands::parseFormatString(const string &format)
{
    FormatString blobs;
    size_t lastpos = 0; // the position where the last plaintext blob started
    for (size_t i = 0; i < format.size(); i++) {
        if (format[i] == '%') {
            if (i + 1 >= format.size()) {
                throw std::invalid_argument(
                    "dangling % at the end of format \"" + format + "\"");
            } else {
                if (i > lastpos) {
                    // add literal text blob
                    blobs.push_back({ true, format.substr(lastpos, i - lastpos)});
                }
                char format_type = format[i+1];
                lastpos = i + 2;
                i++; // also consume the format_type
                if (format_type == '%') {
                    blobs.push_back({true, "%"});
                } else if (format_type == 's') {
                    blobs.push_back({false, "s"});
                } else if (format_type == 'c') {
                    blobs.push_back({false, "c"});
                } else {
                    stringstream msg;
                    msg << "invalid format type %"
                        << format_type << " at position "
                        << i << " in format string \""
                        << format << "\"";
                    throw std::invalid_argument(msg.str());
                }
            }
        }
    }
    if (lastpos < format.size()) {
        blobs.push_back({true, format.substr(lastpos, format.size()-lastpos)});
    }
    return blobs;
}

int MetaCommands::sprintf_cmd(Input input, Output output)
{
    string ident, formatStringSrc;
    if (!(input >> ident >> formatStringSrc)) {
        return HERBST_NEED_MORE_ARGS;
    }
    FormatString format;
    try {
        format = parseFormatString(formatStringSrc);
    }  catch (const std::invalid_argument& e) {
        output << input.command() << ": " << e.what() << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    // evaluate placeholders in the format string
    string replacedString = "";
    for (const auto& blob : format) {
        if (blob.literal_) {
            replacedString += blob.data_;
        } else if (blob.data_ == "c") {
            // a constant string argument
            string constString;
            if (!(input >> constString)) {
                return HERBST_NEED_MORE_ARGS;
            }
            // just copy the plain string to the output
            replacedString += constString;
        } else {
            // we reached %s
            string path;
            if (!(input >> path)) {
                return HERBST_NEED_MORE_ARGS;
            }
            Attribute* a = getAttribute(path, output);
            if (!a) {
                return HERBST_INVALID_ARGUMENT;
            }
            replacedString += a->str();
        }
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
        output << "error: unknown type \"" << typestr << "\"";
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
        output
            << input.command() << ": attribute name must start with \""
            << USER_ATTRIBUTE_PREFIX << "\""
            << " but is actually \"" << attr_name << "\"" << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    if (obj->attribute(attr_name)) {
        output
            << input.command() << ": object \"" << obj_path_and_attr.first.join()
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
            output << input.command() << ": \""
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
        output << input.command() << ": Cannot remove built-in attribute \"" << path << "\"" << endl;
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
            o << "cannot parse \"" << x << "\" to "
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
template<> Finite<CompareOperator>::ValueList Finite<CompareOperator>::values = {
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
        { Type::ATTRIBUTE_INT,      { true,  parse_and_compare<int> }, },
        { Type::ATTRIBUTE_ULONG,    { true,  parse_and_compare<int> }, },
        { Type::ATTRIBUTE_BOOL,     { false, parse_and_compare<bool> }, },
        { Type::ATTRIBUTE_COLOR,    { false, parse_and_compare<Color> }, },
    };
    // the default comparison is simply string based:
    pair<bool, function<int(string,string,Output)>> comparator =
        { false, parse_and_compare<string> };
    auto it = type2compare.find(a->type());
    if (it != type2compare.end()) {
        comparator = it->second;
    }
    if (oper.first && !comparator.first) {
        output << "operator \"" << Converter<CompareOperator>::str(oper) << "\" "
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


void MetaCommands::completeObjectPath(Completion& complete, bool attributes,
                                      function<bool(Attribute*)> attributeFilter)
{
    ArgList objectPathArgs = std::get<0>(Object::splitPath(complete.needle()));
    string objectPath = objectPathArgs.join(OBJECT_PATH_SEPARATOR);
    if (!objectPath.empty()) {
        objectPath += OBJECT_PATH_SEPARATOR;
    }
    Object* object = root.child(objectPathArgs);
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
        output << "No help found for \'" << needle << "\'" << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    return 0;
}

void MetaCommands::helpCompletion(Completion& complete)
{
    completeAttributePath(complete);
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
    // drop output but pass exit code
    return Commands::call(input.fromHere(), dummyOutput);
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
        output << input.command()
               << ": Could not set environment variable: "
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
        output << input.command()
               << ": Could not unset environment variable: "
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

