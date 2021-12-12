#ifndef CLIENTMANAGER_H
#define CLIENTMANAGER_H

#include <X11/X.h>
#include <unordered_map>

#include "commandio.h"
#include "link.h"
#include "object.h"
#include "runtimeconverter.h"
#include "signal.h"

class Client;
class ClientChanges;
class Completion;
class Ewmh;
class HSTag;
class Settings;
class Theme;
class XConnection;

template<>
RunTimeConverter<Client*>* Converter<Client*>::converter;

// Note: this is basically a singleton

class ClientManager : public Object, public Manager<Client>
{
public:
    ClientManager();
    ~ClientManager() override;
    void injectDependencies(Settings* s, Theme* t, Ewmh* e);

    // RunTimeConverter<Monitor*>:
    virtual Client* parse(const std::string& identifier) override;
    virtual std::string str(Client* client) override;
    virtual void completeEntries(Completion& completion) override;

    Client* client(Window window);
    Client* client(const std::string &identifier);
    const std::unordered_map<Window, Client*>&
    clients() { return clients_; }

    void add(Client* client);
    void remove(Window window);

    void unmap_notify(Window win);
    void force_unmanage(Client* client);

    void setDragged(Client* client);

    Signal_<HSTag*> needsRelayout;
    Signal_<Client*> clientStateChanged; //! floating or minimized changed
    Signal_<Client*> clientAdded;
    Link_<Client> focus;
    Link_<Client> dragged;

    int pseudotile_cmd(Input input, Output output);
    int fullscreen_cmd(Input input, Output output);
    void pseudotile_complete(Completion& complete);
    void fullscreen_complete(Completion& complete);

    // adds a new client to list of managed client windows
    Client* manage_client(Window win, bool visible_already, bool force_unmanage,
                          std::function<void(ClientChanges&)> additionalRules = {});
    ClientChanges applyDefaultRules(Window win);

    int applyRulesCmd(Input input, Output output);
    int applyRules(Client* client, Output output, bool changeFocus = true);
    int applyChanges(Client* client, ClientChanges changes, Output output);
    void applyRulesCompletion(Completion& complete);
    int applyTmpRuleCmd(Input input, Output output);
    void applyTmpRuleCompletion(Completion& complete);

protected:
    void focusedClientChanges(Client* newFocus);
    int clientSetAttribute(std::string attribute, Input input, Output output);
    void setSimpleClientAttributes(Client* client, const ClientChanges& changes);
    Theme* theme;
    Settings* settings;
    Ewmh* ewmh;
    XConnection* X_;
    std::unordered_map<Window, Client*> clients_;
    friend class Client;
};

#endif
