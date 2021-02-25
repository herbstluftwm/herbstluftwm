#include "globalcommands.h"

#include "argparse.h"
#include "monitor.h"
#include "monitormanager.h"
#include "root.h"
#include "tag.h"
#include "tagmanager.h"

using std::string;

GlobalCommands::GlobalCommands(Root& root)
    : root_(root)
{
}

int GlobalCommands::tagStatusCommand(Input input, Output output)
{
    Monitor* monitor = root_.monitors->focus();
    ArgParse argparse = ArgParse().optional(monitor);
    if (argparse.parsingAllFails(input, output)) {
        return argparse.exitCode();
    }
    tag_update_flags();
    output << '\t';
    for (size_t i = 0; i < root_.tags->size(); i++) {
        HSTag* tag = root_.tags->byIdx(i);
        // print flags
        char c = '.';
        if (tag->flags & TAG_FLAG_USED) {
            c = ':';
        }
        Monitor* tag_monitor = root_.monitors->byTag(tag);
        if (tag_monitor == monitor) {
            c = '+';
            if (monitor == get_current_monitor()) {
                c = '#';
            }
        } else if (tag_monitor) {
            c = '-';
            if (get_current_monitor() == tag_monitor) {
                c = '%';
            }
        }
        if (tag->flags & TAG_FLAG_URGENT) {
            c = '!';
        }
        output << c;
        output << *tag->name;
        output << '\t';
    }
    return 0;
}

void GlobalCommands::tagStatusCompletion(Completion& complete)
{
    if (complete == 0) {
        Converter<Monitor*>::complete(complete);
    } else {
        complete.none();
    }
}
