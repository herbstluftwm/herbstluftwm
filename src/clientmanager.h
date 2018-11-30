#ifndef CLIENTMANAGER_H
#define CLIENTMANAGER_H

#include "client.h"
#include "object.h"
#include "child.h"
#include "signal.h"

#include <X11/Xlib.h>
#include <unordered_map>
#include "attribute_.h"

class Theme;
class HSClient;
class Settings;

// Note: this is basically a singleton

class ClientManager : public Object
{
public:
    ClientManager(Theme& theme, Settings& settings);
    ~ClientManager() override;

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
    Attribute_<bool> crashOnClient {"crash_on_client", false}; // remove this before merging

    int pseudotile_cmd(Input input, Output output);
    int fullscreen_cmd(Input input, Output output);

    // adds a new client to list of managed client windows
    HSClient* manage_client(Window win, bool visible_already);

protected:
    int clientSetAttribute(std::string attribute, Input input, Output output);
    Theme& theme;
    Settings& settings;
    std::unordered_map<Window, HSClient*> clients_;
    friend class HSClient;
};

#endif
