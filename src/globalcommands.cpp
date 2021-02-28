#include "globalcommands.h"

#include "argparse.h"
#include "client.h"
#include "clientmanager.h"
#include "frametree.h"
#include "layout.h"
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

int GlobalCommands::jumptoCommand(Input input, Output output)
{
    Client* client = nullptr;
    ArgParse argparse = ArgParse().mandatory(client);
    if (argparse.parsingAllFails(input, output)) {
        return argparse.exitCode();
    }
    focus_client(client, true, true, true);
    return 0;
}

void GlobalCommands::jumptoCompletion(Completion& complete)
{
    if (complete == 0) {
        Converter<Client*>::complete(complete);
    } else {
        complete.none();
    }
}

int GlobalCommands::bringCommand(Input input, Output output)
{
    Client* client = nullptr;
    ArgParse argparse = ArgParse().mandatory(client);
    if (argparse.parsingAllFails(input, output)) {
        return argparse.exitCode();
    }
    HSTag* tag = get_current_monitor()->tag;
    root_.tags->moveClient(client, tag, {}, true);
    // mark as un-minimized first, such that a minimized tiling client
    // is added to the frame tree now.
    client->minimized_ = false;
    auto frame = tag->frame->root_->frameWithClient(client);
    if (frame && !frame->isFocused()) {
        // regardless of the client's floating or minimization state:
        // if the client was in the frame tree, it is moved
        // to the focused frame
        frame->removeClient(client);
        tag->frame->focusedFrame()->insertClient(client, true);
    }
    focus_client(client, false, false, true);
    return 0;
}

void GlobalCommands::bringCompletion(Completion& complete)
{
    if (complete == 0) {
        Converter<Client*>::complete(complete);
    } else {
        complete.none();
    }

}
