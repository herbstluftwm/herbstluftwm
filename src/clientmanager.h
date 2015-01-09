#ifndef CLIENTMANAGER_H
#define CLIENTMANAGER_H

#include "object.h"
#include "clientlist.h"

#include <unordered_map>

namespace herbstluft {

// Note: this is basically a singleton

class ClientManager : public Object
{
public:
    ClientManager() : Object("clients") {}
    ~ClientManager();

    std::shared_ptr<HSClient> client(Window window);
    std::shared_ptr<HSClient> client(const std::string &identifier);
    const std::unordered_map<Window, std::shared_ptr<HSClient>>&
    clients() { return clients_; }

    void add(std::shared_ptr<HSClient> client);
    void remove(Window window);

protected:
    std::unordered_map<Window, std::shared_ptr<HSClient>> clients_;
};

}

#endif
