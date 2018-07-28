#include "rootcommands.h"

#include "ipc-protocol.h"
#include "root.h"
#include "command.h"

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


