#include "globalcommands.h"

#include "argparse.h"
#include "client.h"
#include "clientmanager.h"
#include "either.h"
#include "frametree.h"
#include "layout.h"
#include "monitor.h"
#include "monitormanager.h"
#include "root.h"
#include "tag.h"
#include "tagmanager.h"
#include "xconnection.h"

using std::string;

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
                                              [&] (Output output) {
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

