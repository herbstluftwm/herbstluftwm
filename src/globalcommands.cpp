#include "globalcommands.h"

#include "argparse.h"
#include "client.h"
#include "clientmanager.h"
#include "either.h"
#include "frametree.h"
#include "layout.h"
#include "metacommands.h"
#include "monitor.h"
#include "monitormanager.h"
#include "root.h"
#include "settings.h"
#include "tag.h"
#include "tagmanager.h"
#include "xconnection.h"

using std::string;
using std::endl;

GlobalCommands::GlobalCommands(Root& root)
    : root_(root)
{
}

void GlobalCommands::tagStatusCommand(CallOrComplete invoc)
{
    Monitor* monitor = root_.monitors->focus();
    ArgParse().optional(monitor).command(invoc,
        [&] (Output output) {
            tagStatus(monitor, output);
            return 0;
        }
    );
}

void GlobalCommands::tagStatus(Monitor* monitor, Output output)
{
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
}

int GlobalCommands::cycleValueCommand(Input input, Output output)
{
    string attr_path = {};
    ArgParse ap = ArgParse().mandatory(attr_path);
    if (ap.parsingFails(input, output)) {
        return ap.exitCode();
    }
    Attribute* attr = root_.deepAttribute(attr_path);
    if (!attr) {
        // fall back to settings
        attr = root_.settings->deepAttribute(attr_path);
    }
    if (!attr) {
        output << "No such attribute: " << attr_path << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    auto msg = attr->cycleValue(input.begin(), input.end());
    if (!msg.empty()) {
        output << input.command()
               << ": Invalid value for \""
               << attr_path << "\": "
               << msg << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    return 0;
}

void GlobalCommands::cycleValueCompletion(Completion& complete)
{
    if (complete == 0) {
        // only list writable attributes
        MetaCommands::completeObjectPath(complete, &root_, true, &Attribute::writable);
    } else {
        Attribute* attr = root_.deepAttribute(complete[0]);
        if (attr) {
            attr->complete(complete);
        }
    }
}

void GlobalCommands::jumptoCommand(CallOrComplete invoc)
{
    Client* client = nullptr;
    ArgParse().mandatory(client).command(invoc,
        [&] (Output) {
            focus_client(client, true, true, true);
            return 0;
        }
    );
}

void GlobalCommands::bringCommand(CallOrComplete invoc)
{
    Client* client = nullptr;
    ArgParse().mandatory(client).command(invoc,
        [&] (Output) {
            bring(client);
            return 0;
        }
    );
}

void GlobalCommands::bring(Client* client)
{
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
}

void GlobalCommands::raiseCommand(CallOrComplete invoc)
{
    Either<Client*,WindowID> clientOrWin = {nullptr};
    ArgParse().mandatory(clientOrWin).command(invoc,
                                              [&] (Output) {
        auto forClients = [] (Client* client) {
            client->raise();
            client->needsRelayout.emit(client->tag());
        };
        auto forWindowIDs = [&] (WindowID window) {
            XRaiseWindow(root_.X.display(), window);
        };
        clientOrWin.cases(forClients, forWindowIDs);
        return 0;
    });
}


void GlobalCommands::lowerCommand(CallOrComplete invoc)
{
    Either<Client*,WindowID> clientOrWin = {nullptr};
    ArgParse().mandatory(clientOrWin).command(invoc,
                                              [&] (Output) {
        auto forClients = [] (Client* client) {
            client->lower();
            client->needsRelayout.emit(client->tag());
        };
        auto forWindowIDs = [&] (WindowID window) {
            XLowerWindow(root_.X.display(), window);
        };
        clientOrWin.cases(forClients, forWindowIDs);
        return 0;
    });
}

