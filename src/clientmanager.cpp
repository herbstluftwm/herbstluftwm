#include "globals.h"
#include "clientmanager.h"

#include <string>
#include <X11/Xlib.h>
#include "ewmh.h"
#include "root.h"
#include "rules.h"
#include "stack.h"
#include "mouse.h"
#include "monitor.h"
#include "tag.h"
#include "layout.h"

namespace herbstluft {

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

std::shared_ptr<ClientObject> ClientManager::client(Window window)
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
std::shared_ptr<ClientObject> ClientManager::client(const std::string &identifier)
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

void ClientManager::add(std::shared_ptr<ClientObject> client)
{
    clients_[client->window_] = client;
    addChild(client);
}

void ClientManager::remove(Window window)
{
    removeChild(clients_[window]->name());
    clients_.erase(window);
}

std::shared_ptr<ClientObject> manage_client(Window win, bool visible_already) {
    if (is_herbstluft_window(g_display, win)) {
        // ignore our own window
        return NULL;
    }

    auto cm = herbstluft::Root::clients();
    if (cm->client(win)) {
        return NULL;
    }

    // init client
    auto client = std::make_shared<ClientObject>(win);
    client->visible_ = visible_already;
    client->init_from_X();
    HSMonitor* m = get_current_monitor();

    // apply rules
    HSClientChanges changes;
    client_changes_init(&changes, client.get());
    rules_apply(client.get(), &changes);
    if (changes.tag_name) {
        client->setTag(find_tag(changes.tag_name->str));
    }
    if (changes.monitor_name) {
        HSMonitor *monitor = string_to_monitor(changes.monitor_name->str);
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

    // Reuse the keymask string
    client->keymask_ = changes.keymask->str;

    if (!changes.manage) {
        client_changes_free_members(&changes);
        // map it... just to be sure
        XMapWindow(g_display, win);
        return {}; // client gets destroyed
    }

    // actually manage it
    decoration_setup_frame(client.get());
    client->fuzzy_fix_initial_position();
    cm->add(client);
    // insert to layout
    if (!client->tag()) {
        client->setTag(m->tag);
    }
    // insert window to the stack
    client->slice = slice_create_client(client);
    stack_insert_slice(client->tag()->stack, client->slice);
    // insert window to the tag
    client->tag()->frame->lookup(changes.tree_index->str)
                 ->insertClient(client.get());
    if (changes.focus) {
        // give focus to window if wanted
        // TODO: make this faster!
        // WARNING: this solution needs O(C + exp(D)) time where W is the count
        // of clients on this tag and D is the depth of the binary layout tree
        client->tag()->frame->focusClient(client.get());
    }

    ewmh_window_update_tag(client->window_, client->tag());
    tag_set_flags_dirty();
    client->set_fullscreen(changes.fullscreen);
    ewmh_update_window_state(client.get());
    // add client after setting the correct tag for the new client
    // this ensures a panel can read the tag property correctly at this point
    ewmh_add_client(client->window_);

    XSetWindowBorderWidth(g_display, client->window_, 0);
    // specify that the client window survives if hlwm dies, i.e. it will be
    // reparented back to root
    XChangeSaveSet(g_display, client->window_, SetModeInsert);
    XReparentWindow(g_display, client->window_, client->dec.decwin, 40, 40);
    if (client->visible_ == false) client->ignore_unmaps_++;
    // get events from window
    XSelectInput(g_display, client->dec.decwin, (EnterWindowMask | LeaveWindowMask |
                            ButtonPressMask | ButtonReleaseMask |
                            ExposureMask |
                            SubstructureRedirectMask | FocusChangeMask));
    XSelectInput(g_display, win, CLIENT_EVENT_MASK);

    HSMonitor* monitor = find_monitor_with_tag(client->tag());
    if (monitor) {
        if (monitor != get_current_monitor()
            && changes.focus && changes.switchtag) {
            monitor_set_tag(get_current_monitor(), client->tag());
        }
        // TODO: monitor_apply_layout() maybe is called twice here if it
        // already is called by monitor_set_tag()
        monitor_apply_layout(monitor);
        client->set_visible(true);
    } else {
        if (changes.focus && changes.switchtag) {
            monitor_set_tag(get_current_monitor(), client->tag());
            client->set_visible(true);
        }
    }
    client->send_configure();

    client_changes_free_members(&changes);
    grab_client_buttons(client.get(), false);

    return client;
}

void unmanage_client(Window win) {
    auto cm = herbstluft::Root::clients();
    auto client = cm->client(win);
    if (!client) {
        return;
    }
    if (client->dragged_) {
        mouse_stop_drag();
    }
    // remove from tag
    client->tag()->frame->removeClient(client.get());
    // ignore events from it
    XSelectInput(g_display, win, 0);
    //XUngrabButton(g_display, AnyButton, AnyModifier, win);
    // permanently remove it
    XUnmapWindow(g_display, client->dec.decwin);
    XReparentWindow(g_display, win, g_root, 0, 0);
    client->clear_properties();
    HSTag* tag = client->tag();

    client.reset();

    // and arrange monitor after the client has been removed from the stack
    HSMonitor* m = find_monitor_with_tag(tag);
    tag_update_focus_layer(tag);
    if (m) monitor_apply_layout(m);
    ewmh_remove_client(win);
    tag_set_flags_dirty();
    // delete client
    cm->remove(win);
}
}
