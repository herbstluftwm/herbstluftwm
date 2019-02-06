#ifndef CLIENTMANAGER_H
#define CLIENTMANAGER_H

#include <X11/Xlib.h>
#include <unordered_map>

#include "link.h"
#include "client.h"
#include "object.h"
#include "signal.h"

class Theme;
class Client;
class Settings;
class Completion;

// Note: this is basically a singleton

class ClientManager : public Object
{
public:
    ClientManager(Theme& theme, Settings& settings);
    ~ClientManager() override;

    Client* client(Window window);
    Client* client(const std::string &identifier);
    const std::unordered_map<Window, Client*>&
    clients() { return clients_; }

    void add(Client* client);
    void remove(Window window);

    void unmap_notify(Window win);
    void force_unmanage(Window win);
    void force_unmanage(Client* client);

    Signal_<HSTag*> needsRelayout;
    Link_<Client> focus;

    int pseudotile_cmd(Input input, Output output);
    int fullscreen_cmd(Input input, Output output);
    void pseudotile_complete(Completion& complete);
    void fullscreen_complete(Completion& complete);

    // adds a new client to list of managed client windows
    Client* manage_client(Window win, bool visible_already);

protected:
    int clientSetAttribute(std::string attribute, Input input, Output output);
    Theme& theme;
    Settings& settings;
    std::unordered_map<Window, Client*> clients_;
    friend class Client;
};

#endif
