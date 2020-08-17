#include "rootcommands.h"

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
#include "ipc-protocol.h"

using std::endl;
using std::function;
using std::pair;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;

extern char** environ;

RootCommands::RootCommands(Object& root_) : root(root_) {
}

int RootCommands::get_attr_cmd(Input in, Output output) {
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

int RootCommands::set_attr_cmd(Input in, Output output) {
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

int RootCommands::attr_cmd(Input in, Output output) {
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

Attribute* RootCommands::getAttribute(string path, Output output) {
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

int RootCommands::print_object_tree_command(Input in, Output output) {
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

void RootCommands::print_object_tree_complete(Completion& complete) {
    if (complete == 0) {
        completeObjectPath(complete);
    } else {
        complete.none();
    }
}


int RootCommands::substitute_cmd(Input input, Output output)
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

void RootCommands::substitute_complete(Completion& complete)
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

int RootCommands::foreachCmd(Input input, Output output)
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
    // collect the children's name first to ensure that
    // object->childrens() is not changed by the commands we are
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

void RootCommands::foreachComplete(Completion& complete)
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
RootCommands::FormatString RootCommands::parseFormatString(const string &format)
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

int RootCommands::sprintf_cmd(Input input, Output output)
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

void RootCommands::sprintf_complete(Completion& complete)
{
    if (complete == 0) {
        // no completion for arg name
    } else if (complete == 1) {
        // no completion for format string
    } else {
        complete.full(complete[0]); // complete string replacement
        int placeholderCount = 0;
        try {
            FormatString fs = parseFormatString(complete[1]);
            for (const auto& b : fs) {
                if (b.literal_ == false) {
                    placeholderCount++;
                }
            }
        }  catch (const std::invalid_argument&) {
            complete.invalidArguments();
        }
        if (complete < 2 + placeholderCount) {
            completeAttributePath(complete);
        } else {
            complete.completeCommands(2 + placeholderCount);
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

Attribute* RootCommands::newAttributeWithType(string typestr, string attr_name, Output output) {
    auto it = name2constructor.find(typestr);
    if (it == name2constructor.end()) {
        output << "error: unknown type \"" << typestr << "\"";
        return nullptr;
    }
    auto attr = it->second(attr_name);
    attr->setWriteable(true);
    return attr;
}

void RootCommands::completeAttributeType(Completion& complete)
{
    for (const auto& t : name2constructor) {
        complete.full(t.first);
    }
}



int RootCommands::new_attr_cmd(Input input, Output output)
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

void RootCommands::new_attr_complete(Completion& complete)
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

int RootCommands::remove_attr_cmd(Input input, Output output)
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

void RootCommands::remove_attr_complete(Completion& complete) {
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

static std::map<string, pair<bool, vector<int> > > operators {
    // map operator names to "for numeric types only" and possible return codes
    { "=",  { false, { 0 }, }, },
    { "!=", { false, { -1, 1 } }, },
    { "ge", { true, { 1, 0 } }, },
    { "gt", { true, { 1    } }, },
    { "le", { true, { -1, 0 } }, },
    { "lt", { true, { -1    } }, },
};

int RootCommands::compare_cmd(Input input, Output output)
{
    string path, oper, value;
    if (!(input >> path >> oper >> value)) {
        return HERBST_NEED_MORE_ARGS;
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
        { Type::ATTRIBUTE_STRING,   { false, parse_and_compare<string> }, },
        { Type::ATTRIBUTE_BOOL,     { false, parse_and_compare<bool> }, },
        { Type::ATTRIBUTE_COLOR,    { false, parse_and_compare<Color> }, },
    };
    auto it = type2compare.find(a->type());
    if (it == type2compare.end()) {
        output << "attribute " << path << " has unknown type" << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    auto op_it = operators.find(oper);
    if (op_it == operators.end()) {
        output << "unknown operator \"" << oper
            << "\". Possible values are:";
        for (auto i : operators) {
            // only list operators suitable for the attribute type
            if (!it->second.first && i.second.first) {
                continue;
            }
            output << " " << i.first;
        }
        output << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    if (op_it->second.first && !it->second.first) {
        output << "operator \"" << oper << "\" "
            << "only allowed for numeric types, but the attribute "
            << path << " is of non-numeric type "
            << Entity::typestr(a->type()) << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    int comparison_result = it->second.second(a->str(), value, output);
    if (comparison_result > 1) {
        return comparison_result;
    }
    vector<int>& possible_values = op_it->second.second;
    auto found = std::find(possible_values.begin(),
                           possible_values.end(),
                           comparison_result);
    return (found == possible_values.end()) ? 1 : 0;
}

void RootCommands::compare_complete(Completion &complete) {
    if (complete == 0) {
        completeAttributePath(complete);
    } else if (complete == 1) {
        for (auto op : operators) {
            complete.full(op.first);
        }
    } else if (complete == 2) {
        // no completion suggestions for the 'value' field
    } else {
        complete.none();
    }
}


void RootCommands::completeObjectPath(Completion& complete, bool attributes,
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

void RootCommands::completeAttributePath(Completion& complete) {
    completeObjectPath(complete, true);
}

void RootCommands::get_attr_complete(Completion& complete) {
    if (complete == 0) {
        completeAttributePath(complete);
    } else {
        complete.none();
    }
}

void RootCommands::set_attr_complete(Completion& complete) {
    if (complete == 0) {
        completeObjectPath(complete, true,
            [](Attribute* a) { return a->writeable(); } );
    } else if (complete == 1) {
        Attribute* a = root.deepAttribute(complete[0]);
        if (a) {
            a->complete(complete);
        }
    } else {
        complete.none();
    }
}

void RootCommands::attr_complete(Completion& complete)
{
    if (complete == 0) {
        completeAttributePath(complete);
    } else if (complete == 1) {
        Attribute* a = root.deepAttribute(complete[0]);
        if (!a) {
            return;
        }
        if (a->writeable()) {
            a->complete(complete);
        } else {
            complete.none();
        }
    } else {
        complete.none();
    }
}

int RootCommands::tryCommand(Input input, Output output) {
    Commands::call(input.fromHere(), output); // pass output
    return 0; // ignore exit code
}

int RootCommands::silentCommand(Input input, Output output) {
    stringstream dummyOutput;
    // drop output but pass exit code
    return Commands::call(input.fromHere(), dummyOutput);
}

int RootCommands::negateCommand(Input input, Output output)
{
    return ! Commands::call(input.fromHere(), output);
}

void RootCommands::completeCommandShifted1(Completion& complete) {
    complete.completeCommands(0);
}

int RootCommands::echoCommand(Input input, Output output)
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


int RootCommands::setenvCommand(Input input, Output output) {
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

void RootCommands::setenvCompletion(Completion& complete) {
    if (complete == 0) {
        return completeEnvName(complete);
    } else if (complete == 1) {
        // no completion for the value
    } else {
        complete.none();
    }
}

//! a wraper around setenv with the usual 'export' syntax in posix
int RootCommands::exportEnvCommand(Input input, Output output)
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

void RootCommands::exportEnvCompletion(Completion &complete)
{
    for (char** env = environ; *env; ++env) {
        vector<string> chunks = ArgList::split(*env, '=');
        if (!chunks.empty()) {
            complete.partial(chunks[0] + "=");
        }
    }
}

int RootCommands::getenvCommand(Input input, Output output) {
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
void RootCommands::getenvUnsetenvCompletion(Completion& complete) {
    if (complete == 0) {
        return completeEnvName(complete);
    } else {
        complete.none();
    }
}

int RootCommands::unsetenvCommand(Input input, Output output) {
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

void RootCommands::completeEnvName(Completion& complete) {
    for (char** env = environ; *env; ++env) {
        vector<string> chunks = ArgList::split(*env, '=');
        if (!chunks.empty()) {
            complete.full(chunks[0]);
        }
    }
}

int RootCommands::chainCommand(Input input, Output output)
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

void RootCommands::chainCompletion(Completion& complete)
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


vector<vector<string>> RootCommands::splitCommandList(ArgList::Container input) {
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

