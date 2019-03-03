#include "clientmanager.h"

#include <X11/Xlib.h>
#include <string>

#include "client.h"
#include "completion.h"
#include "ewmh.h"
#include "frametree.h"
#include "globals.h"
#include "ipc-protocol.h"
#include "layout.h"
#include "monitor.h"
#include "mouse.h"
#include "root.h"
#include "rulemanager.h"
#include "stack.h"
#include "tag.h"
#include "utils.h"

using std::endl;
using std::string;

ClientManager::ClientManager()
    : focus(*this, "focus")
{
}

ClientManager::~ClientManager()
{
    // make all clients visible at their original floating position
    for (auto c : clients_) {
        auto r = c.second->float_size_;
        auto window = c.second->x11Window();
        XMoveResizeWindow(g_display, window, r.x, r.y, r.width, r.height);
        XReparentWindow(g_display, window, g_root, r.x, r.y);
        ewmh_update_frame_extents(window, 0,0,0,0);
        window_set_visible(window, true);
    }
}

void ClientManager::injectDependencies(Settings* s, Theme* t) {
    settings = s;
    theme = t;
}

Client* ClientManager::client(Window window)
{
    auto entry = clients_.find(window);
    if (entry != clients_.end())
        return entry->second;
    return {};
}

/**
 * \brief   Resolve a window description to a client
 *
 * \param   str     Describes the window: "" means the focused one, "urgent"
 *                  resolves to a arbitrary urgent window, "0x..." just
 *                  resolves to the given window given its hexadecimal window id,
 *                  a decimal number its decimal window id.
 * \return          Pointer to the resolved client, or null, if client not found
 */
Client* ClientManager::client(const string &identifier)
{
    if (identifier.empty()) {
        // TODO: the frame doesn't provide us with a shared pointer yet
        // return get_current_client();
    }
    if (identifier == "urgent") {
        for (auto c : clients_) {
            if (c.second->urgent_)
                return c.second;
        }
        return {}; // no urgent client found
    }
    // try to convert from base 16 or base 10 at the same time
    Window win = std::stoul(identifier, nullptr, 0);
    return client(win);
}

void ClientManager::add(Client* client)
{
    clients_[client->window_] = client;
    client->needsRelayout.connect(needsRelayout);
    addChild(client, client->window_id_str);
}

void ClientManager::remove(Window window)
{
    removeChild(*clients_[window]->window_id_str);
    clients_.erase(window);
}

Client* ClientManager::manage_client(Window win, bool visible_already) {
    if (is_herbstluft_window(g_display, win)) {
        // ignore our own window
        return nullptr;
    }

    if (client(win)) { // if the client is managed already
        return nullptr;
    }

    // init client
    auto client = new Client(win, visible_already, *this);
    Monitor* m = get_current_monitor();

    // apply rules
    ClientChanges changes = Root::get()->rules()->evaluateRules(client);
    if (!changes.tag_name.empty()) {
        client->setTag(find_tag(changes.tag_name.c_str()));
    }
    if (!changes.monitor_name.empty()) {
        Monitor *monitor = string_to_monitor(changes.monitor_name.c_str());
        if (monitor) {
            // a valid tag was not already found, use the target monitor's tag
            if (!client->tag()) { client->setTag(monitor->tag); }
            // a tag was already found, display it on the target monitor, but
            // only if switchtag is set
            else if (changes.switchtag) {
                monitor_set_tag(monitor, client->tag());
            }
        }
    }

    if (changes.pseudotile.has_value()) {
        client->pseudotile_ = changes.pseudotile.value();
    }

    if (changes.ewmhNotify.has_value()) {
        client->ewmhnotify_ = changes.ewmhNotify.value();
    }

    if (changes.ewmhRequests.has_value()) {
        client->ewmhrequests_ = changes.ewmhRequests.value();
    }

    // Reuse the keymask string
    client->keyMask_ = changes.keyMask;

    if (!changes.manage) {
        // map it... just to be sure
        XMapWindow(g_display, win);
        return {}; // client gets destroyed
    }

    // actually manage it
    client->dec.createWindow();
    client->fuzzy_fix_initial_position();
    add(client);
    // insert to layout
    if (!client->tag()) {
        client->setTag(m->tag);
    }
    // insert window to the stack
    client->slice = Slice::makeClientSlice(client);
    client->tag()->stack->insertSlice(client->slice);
    // insert window to the tag
    FrameTree::focusedFrame(client->tag()->frame->lookup(changes.tree_index))
                 ->insertClient(client);
    if (changes.focus) {
        // give focus to window if wanted
        // TODO: make this faster!
        // WARNING: this solution needs O(C + exp(D)) time where W is the count
        // of clients on this tag and D is the depth of the binary layout tree
        client->tag()->frame->focusClient(client);
    }

    ewmh_window_update_tag(client->window_, client->tag());
    tag_set_flags_dirty();
    client->set_fullscreen(changes.fullscreen);
    ewmh_update_window_state(client);
    // add client after setting the correct tag for the new client
    // this ensures a panel can read the tag property correctly at this point
    ewmh_add_client(client->window_);

    client->make_full_client();

    Monitor* monitor = find_monitor_with_tag(client->tag());
    if (monitor) {
        if (monitor != get_current_monitor()
            && changes.focus && changes.switchtag) {
            monitor_set_tag(get_current_monitor(), client->tag());
        }
        // TODO: monitor_apply_layout() maybe is called twice here if it
        // already is called by monitor_set_tag()
        monitor->applyLayout();
        client->set_visible(true);
    } else {
        if (changes.focus && changes.switchtag) {
            monitor_set_tag(get_current_monitor(), client->tag());
            client->set_visible(true);
        }
    }
    client->send_configure();

    grab_client_buttons(client, false);

    return client;
}

void ClientManager::unmap_notify(Window win) {
    auto client = this->client(win);
    if (!client) {
        return;
    }
    if (!client->ignore_unmapnotify()) {
        force_unmanage(client);
    }
}

void ClientManager::force_unmanage(Client* client) {
    if (client->dragged_) {
        mouse_stop_drag();
    }
    if (client->tag() && client->slice) {
        client->tag()->stack->removeSlice(client->slice);
    }
    // remove from tag
    client->tag()->frame->root_->removeClient(client);
    // ignore events from it
    XSelectInput(g_display, client->window_, 0);
    //XUngrabButton(g_display, AnyButton, AnyModifier, win);
    // permanently remove it
    XUnmapWindow(g_display, client->dec.decorationWindow());
    XReparentWindow(g_display, client->window_, g_root, 0, 0);
    client->clear_properties();
    HSTag* tag = client->tag();


    // and arrange monitor after the client has been removed from the stack
    tag_update_focus_layer(tag);
    needsRelayout.emit(tag);
    ewmh_remove_client(client->window_);
    tag_set_flags_dirty();
    // delete client
    this->remove(client->window_);
    delete client;
}

int ClientManager::clientSetAttribute(string attribute,
                                      Input input,
                                      Output output)
{
    string value = input.empty() ? "toggle" : input.front();
    Client* c = get_current_client();
    if (c) {
        Attribute* a = c->attribute(attribute);
        if (!a) return HERBST_UNKNOWN_ERROR;
        string error_message = a->change(value);
        if (error_message != "") {
            output << input.command() << ": illegal argument \""
                   << value << "\": "
                   << error_message << endl;
            return HERBST_INVALID_ARGUMENT;
        }
    }
    return 0;
}

int ClientManager::pseudotile_cmd(Input input, Output output)
{
    return clientSetAttribute("pseudotile", input, output);
}

int ClientManager::fullscreen_cmd(Input input, Output output)
{
    return clientSetAttribute("fullscreen", input, output);
}

void ClientManager::pseudotile_complete(Completion& complete)
{
    fullscreen_complete(complete);
}

void ClientManager::fullscreen_complete(Completion& complete)
{
    if (complete == 0) {
        // we want this command to have a completion, even if no client
        // is focused at the moment.
        bool value = true;
        Converter<bool>::complete(complete, &value);
    } else {
        complete.none();
    }
}

