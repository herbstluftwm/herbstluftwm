#include "clientmanager.h"

#include <X11/Xlib.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

#include "attribute.h"
#include "client.h"
#include "completion.h"
#include "decoration.h"
#include "ewmh.h"
#include "ipc-protocol.h"
#include "monitor.h"
#include "monitormanager.h"
#include "mousemanager.h"
#include "root.h"
#include "rulemanager.h"
#include "stack.h"
#include "tag.h"
#include "tagmanager.h"
#include "utils.h"
#include "xconnection.h"

using std::endl;
using std::function;
using std::string;
using std::vector;

template<>
RunTimeConverter<Client*>* Converter<Client*>::converter = nullptr;

ClientManager::ClientManager()
    : focus(*this, "focus")
    , dragged(*this, "dragged")
    , theme(nullptr)
    , settings(nullptr)
    , ewmh(nullptr)
    , X_(nullptr)
{
    setDoc("The managed windows. For every (managed) window id there "
           "is an entry here.");
    focus.setDoc("the focused client (only exists if a client is focused)");
    dragged.setDoc("the object of a client which is currently dragged"
                   " by the mouse, if any. See the documentation of the"
                   " mousebind command for examples.");

    focus.changed().connect(this, &ClientManager::focusedClientChanges);
}

ClientManager::~ClientManager()
{
    // make all clients visible at their original floating position
    for (auto c : clients_) {
        Rectangle r = c.second->float_size_;
        auto window = c.second->x11Window();
        XMoveResizeWindow(X_->display(), window, r.x, r.y, r.width, r.height);
        XReparentWindow(X_->display(), window, X_->root(), r.x, r.y);
        ewmh->updateFrameExtents(window, 0,0,0,0);
        XMapWindow(X_->display(), window);
        delete c.second;
    }
}

void ClientManager::injectDependencies(Settings* s, Theme* t, Ewmh* e) {
    settings = s;
    theme = t;
    ewmh = e;
    X_ = &e->X();
}

string ClientManager::str(Client* client)
{
    return client->window_id_str;
}

Client* ClientManager::client(Window window)
{
    auto entry = clients_.find(window);
    if (entry != clients_.end()) {
        return entry->second;
    }
    return {};
}

/**
 * \brief   Resolve a window description to a client. If there is
 *          no such client, throw an exception.
 *
 * \param   str     Describes the window: "" means the focused one, "urgent"
 *                  resolves to a arbitrary urgent window, "0x..." just
 *                  resolves to the given window given its hexadecimal window id,
 *                  a decimal number its decimal window id.
 * \return          Pointer to the resolved client.
 */
Client* ClientManager::parse(const string& identifier)
{
    if (identifier.empty()) {
        Client* c = focus();
        if (c) {
            return c;
        } else {
            throw std::invalid_argument("No client is focused");
        }
    }
    if (identifier == "urgent") {
        for (auto c : clients_) {
            if (c.second->urgent_) {
                return c.second;
            }
        }
        throw std::invalid_argument("No client is urgent");
    }
    if (identifier == "last-minimized" || identifier == "longest-minimized") {
        bool oldest = identifier == "longest-minimized";
        Client *c = Root::get()->tags()->focus_->minimizedClient(oldest);
        if (c) {
            return c;
        } else {
            throw std::invalid_argument("No client is minimized");
        }
    }
    Window win = {};
    try {
        win = Converter<WindowID>::parse(identifier);
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid format, expecting 0xWINID or a description, e.g. \'urgent\'.");
    }
    auto entry = clients_.find(win);
    if (entry != clients_.end()) {
        return entry->second;
    } else {
        throw std::invalid_argument("No managed client with window id " + identifier);
    }
}

Client* ClientManager::client(const string &identifier) {
    try {
        return parse(identifier);
    }  catch (...) {
        return nullptr;
    }
}

//! the completion-counterpart of ClientManager::client()
void ClientManager::completeEntries(Completion& complete)
{
    complete.full("urgent");
    complete.full("last-minimized");
    complete.full("longest-minimized");
    for (const auto& it : clients_) {
        complete.full(Converter<WindowID>::str(it.first));
    }
}

void ClientManager::add(Client* client)
{
    clients_[client->window_] = client;
    client->needsRelayout.connect(needsRelayout);
    client->floating_.changed().connect([this,client]() {
        this->clientStateChanged.emit(client);
    });
    client->minimized_.changed().connect([this,client]() {
        this->clientStateChanged.emit(client);
    });
    addChild(client, client->window_id_str);
    clientAdded.emit(client);
}

void ClientManager::setDragged(Client* client) {
    if (dragged()) {
        dragged()->dragged_ = false;
    }
    dragged = client;
    if (dragged()) {
        dragged()->dragged_ = true;
    }
}

void ClientManager::remove(Window window)
{
    removeChild(*clients_[window]->window_id_str);
    clients_.erase(window);
}

Client* ClientManager::manage_client(Window win, bool visible_already, bool force_unmanage,
                                     function<void(ClientChanges&)> additionalRules) {
    if (is_herbstluft_window(X_->display(), win)) {
        // ignore our own window
        return nullptr;
    }

    if (client(win)) { // if the client is managed already
        return nullptr;
    }

    // init client
    auto client = new Client(win, visible_already, *this);
    client->listen_for_events();
    Monitor* m = get_current_monitor();

    // apply rules
    ClientChanges changes = applyDefaultRules(client->window_);
    if (additionalRules) {
        additionalRules(changes);
    }
    auto stdio = OutputChannels::stdio();
    changes = Root::get()->rules()->evaluateRules(client, stdio, changes);
    if (!changes.manage || force_unmanage) {
        // if the window becomes unmanaged and wasn't visible before,
        // then map it.
        if (!visible_already) {
            XMapWindow(X_->display(), win);
        }
        delete client;
        return {};
    }

    if (!changes.tag_name.empty()) {
        HSTag* tag = find_tag(changes.tag_name.c_str());
        if (tag) {
            client->setTag(tag);
        }
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

    // important that this happens befor the insertion to a tag
    setSimpleClientAttributes(client, changes);

    // actually manage it
    client->dec->createWindow();
    client->fuzzy_fix_initial_position();
    add(client);
    // insert to layout
    if (!client->tag()) {
        client->setTag(m->tag);
    }
    // insert window to the stack
    client->slice = Slice::makeClientSlice(client);
    client->tag()->insertClientSlice(client);
    // insert window to the tag
    client->tag()->insertClient(client, changes.tree_index, changes.focus);

    tag_set_flags_dirty();
    if (changes.fullscreen.has_value()) {
        client->fullscreen_ = changes.fullscreen.value();
    } else {
        client->fullscreen_ = ewmh->isFullscreenSet(client->window_);
    }
    ewmh->updateWindowState(client);
    // add client after setting the correct tag for the new client
    // this ensures a panel can read the tag property correctly at this point
    ewmh->addClient(client->window_);

    client->make_full_client();

    Monitor* monitor = find_monitor_with_tag(client->tag());
    if (monitor) {
        if (monitor != get_current_monitor()
            && changes.focus && changes.switchtag) {
            monitor_set_tag(get_current_monitor(), client->tag());
        }
        monitor->evaluateClientPlacement(client, changes.floatplacement);
        // TODO: monitor_apply_layout() maybe is called twice here if it
        // already is called by monitor_set_tag()
        monitor->applyLayout();
        client->set_visible(true);
    } else {
        if (changes.focus && changes.switchtag) {
            monitor_set_tag(get_current_monitor(), client->tag());
            get_current_monitor()->evaluateClientPlacement(client, changes.floatplacement);
            client->set_visible(true);
        } else {
            // if the client is not directly displayed on any monitor,
            // take the current monitor
            get_current_monitor()->evaluateClientPlacement(client, changes.floatplacement);
            // mark the client as hidden
            ewmh->windowUpdateWmState(client->window_, WmState::WSIconicState);
        }
    }
    client->send_configure(true);

    // TODO: make this better
    Root::get()->mouse->grab_client_buttons(client, false);

    return client;
}

//! apply some built in rules that reflect the EWMH specification
//! and regarding sensible single-window floating settings
ClientChanges ClientManager::applyDefaultRules(Window win)
{
    ClientChanges changes;
    const int windowType = ewmh->getWindowType(win);
    vector<int> unmanaged= {
        NetWmWindowTypeDesktop,
        NetWmWindowTypeDock,
    };
    if (std::find(unmanaged.begin(), unmanaged.end(), windowType)
            != unmanaged.end())
    {
        changes.manage = False;
    }
    vector<int> floated = {
        NetWmWindowTypeToolbar,
        NetWmWindowTypeMenu,
        NetWmWindowTypeUtility,
        NetWmWindowTypeSplash,
        NetWmWindowTypeDialog,
        NetWmWindowTypeDropdownMenu,
        NetWmWindowTypePopupMenu,
        NetWmWindowTypeTooltip,
        NetWmWindowTypeNotification,
        NetWmWindowTypeCombo,
        NetWmWindowTypeDnd,
    };
    if (std::find(floated.begin(), floated.end(), windowType) != floated.end())
    {
        changes.floating = True;
    }
    if (X_->getTransientForHint(win).has_value()) {
        changes.floating = true;
    }
    return changes;
}

/** apply simple attribute based client changes. We do not apply 'fullscreen' here because
 * its value defaults to the client's ewmh property and is handled in applyRulesCmd() and manage_client() differently.
 */
void ClientManager::setSimpleClientAttributes(Client* client, const ClientChanges& changes)
{
    if (changes.decorated.has_value()) {
        client->decorated_ = changes.decorated.value();
    }
    if (changes.floating.has_value()) {
        client->floating_ = changes.floating.value();
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

    if (changes.keyMask.has_value()) {
        client->keyMask_ = changes.keyMask.value();
    }
    if (changes.keysInactive.has_value()) {
        client->keysInactive_ = changes.keysInactive.value();
    }
    if (changes.floatingGeometry.has_value()) {
        Rectangle geo = changes.floatingGeometry.value();
        // do not simply copy the geometry to the attribute
        // but possibly apply the size hints:
        if (client->sizehints_floating_()) {
            client->applysizehints(&geo.width, &geo.height, true);
        }
        client->float_size_ = geo;
    }
}

int ClientManager::applyRulesCmd(Input input, Output output) {
    string winid;
    if (!(input >> winid)) {
        return HERBST_NEED_MORE_ARGS;
    }
    if (winid == "--all") {
        MonitorManager* monitors = Root::get()->monitors();
        monitors->lock(); // avoid unnecessary redraws
        int status = 0;
        for (const auto& it : clients_) {
            status = std::max(status, applyRules(it.second, output, false));
        }
        monitors->unlock();
        return status;
    } else {
        Client* client = this->client(winid);
        if (!client) {
            output.perror() << "No such (managed) client: " << winid << "\n";
            return HERBST_INVALID_ARGUMENT;
        }
        return applyRules(client, output);
    }
}

//! apply all rules for the given client. if focus=on and changeFocus=true,
//! then the client is focused
int ClientManager::applyRules(Client* client, Output output, bool changeFocus)
{
    ClientChanges changes;
    changes.focus = client == focus();
    changes = Root::get()->rules()->evaluateRules(client, output, changes);
    if (!changeFocus) {
        changes.focus = false;
    }
    return applyChanges(client, changes, output);
}

int ClientManager::applyChanges(Client* client, ClientChanges changes, Output output)
{
    if (changes.manage == false) {
        // only make unmanaging clients possible as soon as it is
        // possible to make them managed again
        output.perror() << "Unmanaging clients not yet possible.\n";
        return HERBST_INVALID_ARGUMENT;
    }
    // do the simple attributes first
    setSimpleClientAttributes(client, changes);
    bool clientNeedsRelayout = false;
    if (changes.floatingGeometry.has_value()) {
        // if the floating geometry changed, make sure
        // that the client's position is updated
        clientNeedsRelayout = true;
    }
    if (changes.fullscreen.has_value()) {
        client->fullscreen_ = changes.fullscreen.value();
    }
    HSTag* tag = nullptr;
    Monitor* monitor = nullptr;
    bool switch_tag = false;
    // in the following, we do the same decisions in the same order as in manage_client();
    // the only difference is, that we only set the above variables and execute the decisions
    // later
    if (!changes.tag_name.empty()) {
        tag = find_tag(changes.tag_name.c_str());
    }
    if (!changes.monitor_name.empty()) {
        monitor = string_to_monitor(changes.monitor_name.c_str());
        if (monitor) {
            // a valid tag was not already found, use the target monitor's tag
            if (!tag) { tag = monitor->tag; }
            // a tag was already found, display it on the target monitor, but
            // only if switchtag is set
            else if (changes.switchtag) {
                switch_tag = true;
            }
        }
    }
    if (tag || !changes.tree_index.empty()) {
        if (!tag) {
            tag = client->tag();
        }
        TagManager* tagman = Root::get()->tags();
        tagman->moveClient(client, tag, changes.tree_index, changes.focus);
        clientNeedsRelayout = false; // the above relayouts the tag
    } else if (changes.focus && (client != focus())) {
        // focus the client
        client->tag()->focusClient(client);
        Root::get()->monitors->relayoutTag(client->tag());
        clientNeedsRelayout = false; // the above relayouts the tag
    }
    if (monitor && switch_tag && tag) {
        monitor_set_tag(monitor, tag);
    }
    if (clientNeedsRelayout) {
        // if the client still has not been resized yet:
        Root::get()->monitors->relayoutTag(client->tag());
    }
    return 0;
}

void ClientManager::applyRulesCompletion(Completion& complete)
{
    if (complete == 0) {
        complete.full("--all");
        completeEntries(complete);
    } else {
        complete.none();
    }
}

int ClientManager::applyTmpRuleCmd(Input input, Output output)
{
    string clientStr;
    if (!(input >> clientStr)) {
        return HERBST_NEED_MORE_ARGS;
    }
    // 'applyTo' is a pointer to a map containing those to which the
    // above rule shall be applied
    std::unordered_map<Window, Client*> singletonMap;
    std::unordered_map<Window, Client*>* applyTo = &singletonMap;
    if (clientStr == "--all") {
        // apply to all clients
        applyTo = &clients_;
    } else {
        // use the 'singletonMap' if the rule shall be applied
        // to only one client
        Client* client = this->client(clientStr);
        if (!client) {
            output.perror() << "No such (managed) client: " << clientStr << "\n";
            return HERBST_INVALID_ARGUMENT;
        }
        (*applyTo)[client->x11Window()] = client;
    }
    // parse the rule
    bool prepend = false;
    Rule rule;
    int status = RuleManager::parseRule(input, output, rule, prepend);
    if (status != 0) {
        return status;
    }
    for (auto& it : *applyTo) {
        Client* client = it.second;
        ClientChanges changes;
        changes.focus = client == focus();
        rule.evaluate(client, changes, output);
        if (applyTo->size() > 1) {
            // if we apply the rule to more than one
            // client, then we leave the focus where it was
            changes.focus = false;
        }
        applyChanges(client, changes, output);
    }
    return 0;
}

void ClientManager::applyTmpRuleCompletion(Completion& complete)
{
    if (complete == 0) {
        complete.full("--all");
        completeEntries(complete);
    } else {
        Root::get()->rules->addRuleCompletion(complete);
    }
}

/**
 * @brief called whenever clients.focus changes
 * @param newFocus
 */
void ClientManager::focusedClientChanges(Client* newFocus)
{
    if (newFocus) {
        hook_emit({"focus_changed", newFocus->window_id_str(), newFocus->title_()});
    } else {
        hook_emit({"focus_changed", "0x0", ""});
    }
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
    if (dragged() == client) {
        dragged = nullptr;
        Root::get()->mouse->mouse_stop_drag();
    }
    if (client->tag() && client->slice) {
        client->tag()->stack->removeSlice(client->slice);
    }
    // remove from tag
    client->tag()->removeClient(client);
    // ignore events from it
    XSelectInput(X_->display(), client->window_, 0);
    //XUngrabButton(X_->display(), AnyButton, AnyModifier, win);
    // permanently remove it
    XUnmapWindow(X_->display(), client->decorationWindow());
    XReparentWindow(X_->display(), client->window_, X_->root(), 0, 0);
    client->clear_properties();
    HSTag* tag = client->tag();


    // and arrange monitor after the client has been removed from the stack
    needsRelayout.emit(tag);
    ewmh->removeClient(client->window_);
    tag_set_flags_dirty();
    // delete client
    this->remove(client->window_);
    if (client == focus()) {
        // this should never happen because we forced a relayout
        // of the client's tag, so 'focus' must have been updated
        // in the meantime. Anyway, lets be safe:
        focus = nullptr;
    }
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
        if (!a) {
            return HERBST_UNKNOWN_ERROR;
        }
        string error_message = a->change(value);
        if (!error_message.empty()) {
            output.perror() << "illegal argument \""
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

