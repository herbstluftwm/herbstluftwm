#ifndef CLIENTMANAGER_H
#define CLIENTMANAGER_H

#include "object.h"
#include "client.h"
#include "clientobject.h"

#include <unordered_map>

namespace herbstluft {

// Note: this is basically a singleton

class ClientManager : public Object
{
public:
    ClientManager() : Object("clients") {}
    ~ClientManager();

    std::shared_ptr<ClientObject> client(Window window);
    std::shared_ptr<ClientObject> client(const std::string &identifier);
    const std::unordered_map<Window, std::shared_ptr<ClientObject>>&
    clients() { return clients_; }

    void add(std::shared_ptr<ClientObject> client);
    void remove(Window window);

protected:
    std::unordered_map<Window, std::shared_ptr<ClientObject>> clients_;
};

// adds a new client to list of managed client windows
std::shared_ptr<ClientObject> manage_client(Window win, bool visible_already);
void unmanage_client(Window win);


}

#endif
