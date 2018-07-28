#include "rootcommands.h"

#include "ipc-protocol.h"
#include "root.h"
#include "command.h"
#include "attribute_.h"

#include <map>
#include <functional>

using namespace std;

int substitute_cmd(Root* root, Input input, Output output)
{
    string cmd, ident, path;
    if (!input.read({ &cmd, &ident, &path })) {
        return HERBST_NEED_MORE_ARGS;
    }
    if (input.empty()) return HERBST_NEED_MORE_ARGS;
    Attribute* a = root->getAttribute(path, output);
    if (!a) return HERBST_INVALID_ARGUMENT;
    return Commands::call(input.replace(ident, a->str()), output);
}

int sprintf_cmd(Root* root, Input input, Output output)
{
    string cmd, ident, format;
    if (!input.read({ &cmd, &ident, &format })) return HERBST_NEED_MORE_ARGS;
    string blobs;
    size_t lastpos = 0; // the position where the last plaintext blob started
    for (size_t i = 0; i < format.size(); i++) if (format[i] == '%') {
        if (i + 1 >= format.size()) {
            output
                << cmd << ": dangling % at the end of format \""
                << format << "\"" << endl;
            return HERBST_INVALID_ARGUMENT;
        } else {
            if (i > lastpos) {
                blobs += format.substr(lastpos, i - lastpos);
            }
            char format_type = format[i+1];
            lastpos = i + 2;
            i++; // also consume the format_type
            if (format_type == '%') {
                blobs += "%";
            } else if (format_type == 's') {
                string path;
                if (!input.read({ &path })) {
                    return HERBST_NEED_MORE_ARGS;
                }
                Attribute* a = root->getAttribute(path, output);
                if (!a) return HERBST_INVALID_ARGUMENT;
                blobs += a->str();
            } else {
                output
                    << cmd << ": invalid format type %"
                    << format_type << " at position "
                    << i << " in format string \""
                    << format << "\"" << endl;
                return HERBST_INVALID_ARGUMENT;
            }
        }
    }
    if (lastpos < format.size()) {
        blobs += format.substr(lastpos, format.size()-lastpos);
    }
    return Commands::call(input.replace(ident, blobs), output);
}

int new_attr_cmd(Root* root, Input input, Output output)
{
    string cmd, type, path;
    if (!input.read({ &cmd, &type, &path })) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto obj_path_and_attr = Object::splitPath(path);
    string attr_name = obj_path_and_attr.second;
    Object* obj = root->child(obj_path_and_attr.first, output);
    if (!obj) return HERBST_INVALID_ARGUMENT;
    if (attr_name.substr(0,strlen(USER_ATTRIBUTE_PREFIX)) != USER_ATTRIBUTE_PREFIX) {
        output
            << cmd << ": attribute name must start with \""
            << USER_ATTRIBUTE_PREFIX << "\""
            << " but is actually \"" << attr_name << "\"" << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    if (NULL != obj->attribute(attr_name)) {
        output
            << cmd << ": object \"" << obj_path_and_attr.first.join()
            << "\" already has an attribute named \"" << attr_name
            <<  "\"" << endl;
        return HERBST_INVALID_ARGUMENT;
    }

    std::map<string, function<Attribute*(string)>> name2constructor {
    { "bool",  [](string n) { return new Attribute_<bool>(n, false); }},
    { "color", [](string n) { return new Attribute_<Color>(n, Color("black")); }},
    { "int",   [](string n) { return new Attribute_<int>(n, 0); }},
    { "string",[](string n) { return new Attribute_<string>(n, ""); }},
    { "uint",  [](string n) { return new Attribute_<unsigned long>(n, 0); }},
    };
    auto it = name2constructor.find(type);
    if (it == name2constructor.end()) {
        output << cmd << ": unknown type \"" << type << "\"";
        return HERBST_INVALID_ARGUMENT;
    }
    // create the new attribute and add it
    Attribute* a = it->second(attr_name);
    obj->addAttribute(a);
    return 0;
}

