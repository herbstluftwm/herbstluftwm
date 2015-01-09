#include "globals.h"
#include "clientmanager.h"

#include <string>
#include <X11/Xlib.h>
#include "ewmh.h"

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

std::shared_ptr<HSClient> ClientManager::client(Window window)
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
std::shared_ptr<HSClient> ClientManager::client(const std::string &identifier)
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

void ClientManager::add(std::shared_ptr<HSClient> client)
{
    clients_[client->window_] = client;
    addChild(client);
}

void ClientManager::remove(Window window)
{
    clients_.erase(window);
    removeChild(clients_[window]->name());
}

}
