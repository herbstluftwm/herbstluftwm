#include "globalcommands.h"

#include "argparse.h"
#include "client.h"
#include "clientmanager.h"
#include "command.h"
#include "either.h"
#include "ewmh.h"
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

int GlobalCommands::focusEdgeCommand(Input input, Output output)
{
    // Puts the focus to the edge in the specified direction
    root_.monitors->lock();
    int oldval = root_.settings->focus_crosses_monitor_boundaries();
    root_.settings->focus_crosses_monitor_boundaries = false;
    Input inp = {"focus", input.toVector()};
    while (0 == Commands::call(inp, output)) {
    }
    root_.settings->focus_crosses_monitor_boundaries = oldval;
    root_.monitors->unlock();
    return 0;
}

int GlobalCommands::shiftEdgeCommand(Input input, Output output)
{
    // Moves a window to the edge in the specified direction
    root_.monitors->lock();
    int oldval = root_.settings->focus_crosses_monitor_boundaries();
    root_.settings->focus_crosses_monitor_boundaries = false;
    Input inp = {"shift", input.toVector()};
    while (0 == Commands::call(inp, output)) {
    }
    root_.settings->focus_crosses_monitor_boundaries = oldval;
    root_.monitors->unlock();
    return 0;
}

void GlobalCommands::focusEdgeCompletion(Completion& complete)
{
    root_.monitors->focus()->tag->focusInDirCommand(
                CallOrComplete::complete(complete));
}

void GlobalCommands::shiftEdgeCompletion(Completion& complete)
{
    root_.monitors->focus()->tag->shiftInDirCommand(
                CallOrComplete::complete(complete));
}

void GlobalCommands::useTagCommand(CallOrComplete invoc)
{
    HSTag* tag = nullptr;
    ArgParse()
            .mandatory(tag)
            .command(invoc,
                     [&](Output output) {
        Monitor* monitor = root_.monitors->focus();
        int ret = monitor_set_tag(monitor, tag);
        if (ret != 0) {
            output << invoc.command() << ": Could not change tag";
            if (monitor->lock_tag) {
                output << " (monitor " << monitor->index() << " is locked)";
            }
            output << "\n";
        }
        return ret;
    });

}

void GlobalCommands::useTagByIndexCommand(CallOrComplete invoc)
{
    string indexStr = "";
    bool skipVisible = false;
    ArgParse().mandatory(indexStr, {"-1", "+1"})
            .flags({{"--skip-visible", &skipVisible}})
            .command(invoc,
                     [&] (Output output) -> int {
        HSTag* tag = root_.tags->byIndexStr(indexStr, skipVisible);
        if (!tag) {
            output << invoc.command() <<
                ": Invalid index \"" << indexStr << "\"\n";
            return HERBST_INVALID_ARGUMENT;
        }
        Monitor* monitor = root_.monitors->focus();
        int ret = monitor_set_tag(monitor, tag);
        if (ret != 0) {
            output << invoc.command() << ": Could not change tag";
            if (monitor->lock_tag) {
                output << " (monitor " << monitor->index() << " is locked)";
            }
            output << "\n";
        }
        return ret;
    });

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

void GlobalCommands::usePreviousCommand(CallOrComplete invoc)
{
    ArgParse().command(invoc,
                       [&] (Output output) {
        Monitor* monitor = root_.monitors->focus();
        HSTag* tag = monitor->tag_previous;
        HSAssert(tag);
        int ret = monitor_set_tag(monitor, tag);
        if (ret != 0) {
            output << "use_previous: Could not change tag";
            if (monitor->lock_tag()) {
                output << " (monitor is locked)";
            }
            output << "\n";
        }
        return ret;
    });
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

void GlobalCommands::closeCommand(CallOrComplete invoc)
{
    Either<Client*,WindowID> clientOrWin = { nullptr };
    ArgParse()
            .optional(clientOrWin)
            .command(invoc, [&] (Output) {
        return clientOrWin.cases<int>([&] (Client* c) {
            if (!c) {
                c = root_.clients->focus();
            }
            if (!c) {
                return HERBST_INVALID_ARGUMENT;
            }
            c->requestClose();
            return HERBST_EXIT_SUCCESS;
        },
        [&] (WindowID win) {
            // if the window ID of an unmanaged
            // client was given
            root_.ewmh_.windowClose(win);
            return HERBST_EXIT_SUCCESS;
        });
    });
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

void GlobalCommands::focusNthCommand(CallOrComplete invoc)
{
    // essentially an alias to:
    // set_attr tags.focus.tiling.focused_frame.selection
    // with the additional feature that setSelection() allows
    // to directly focus the last window in a frame.
    int index = 0;
    ArgParse().mandatory(index, {"0", "-1"})
            .command(invoc, [&](Output) {
        HSTag* tag = root_.tags->focus_();
        auto frame = tag->frame->focusedFrame();
        frame->setSelection(index);
        tag->needsRelayout_.emit();
        return 0;
    });
}

