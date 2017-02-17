#ifndef CLIENTMANAGER_H
#define CLIENTMANAGER_H

#include "object.h"
#include "client.h"

#include <unordered_map>


// Note: this is basically a singleton

class ClientManager : public Object
{
public:
    ClientManager() {}
    ~ClientManager();

    std::shared_ptr<HSClient> client(Window window);
    std::shared_ptr<HSClient> client(const std::string &identifier);
    const std::unordered_map<Window, std::shared_ptr<HSClient>>&
    clients() { return clients_; }

    void add(std::shared_ptr<HSClient> client);
    void remove(Window window);

    void unmap_notify(Window win);
    void force_unmanage(Window win);
    void force_unmanage(std::shared_ptr<HSClient> client);

protected:
    std::unordered_map<Window, std::shared_ptr<HSClient>> clients_;
};

// adds a new client to list of managed client windows
std::shared_ptr<HSClient> manage_client(Window win, bool visible_already);



#endif
