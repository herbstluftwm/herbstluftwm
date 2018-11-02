#ifndef CLIENTMANAGER_H
#define CLIENTMANAGER_H

#include "object.h"
#include "child.h"
#include "signal.h"

#include <unordered_map>

class Theme;
class HSClient;
class Settings;

// Note: this is basically a singleton

class ClientManager : public Object
{
public:
    ClientManager(Theme& theme, Settings& settings);
    ~ClientManager();

    HSClient* client(Window window);
    HSClient* client(const std::string &identifier);
    const std::unordered_map<Window, HSClient*>&
    clients() { return clients_; }

    void add(HSClient* client);
    void remove(Window window);

    void unmap_notify(Window win);
    void force_unmanage(Window win);
    void force_unmanage(HSClient* client);

    Signal_<HSTag*> needsRelayout;
    Child_<HSClient> focus;

    // adds a new client to list of managed client windows
    HSClient* manage_client(Window win, bool visible_already);

protected:
    Theme& theme;
    Settings& settings;
    std::unordered_map<Window, HSClient*> clients_;
    friend class HSClient;
};

#endif
