#pragma once

#include <X11/X.h>
#include <list>
#include <map>
#include <memory>

#include "mouse.h"
#include "object.h"
#include "optional.h"

class Completion;
class ClientManager;
class MonitorManager;
class MouseDragHandler;
struct Point2D;

class MouseManager : public Object {
public:
    MouseManager();
    ~MouseManager();

    void injectDependencies(ClientManager* clients, MonitorManager* monitors);

    int addMouseBindCommand(Input input, Output output);

    void addMouseBindCompletion(Completion &complete);

    int mouse_unbind_all(Output o);

    void grab_client_buttons(Client* client, bool focused);

    bool mouse_handle_event(unsigned int modifiers, unsigned int button, Window window);
    void mouse_stop_drag();
    bool mouse_is_dragging();
    void handle_motion_event(Point2D newCursorPos);

    int dragCommand(Input input, Output output);
    void dragCompletion(Completion& complete);

    std::string mouse_initiate_move(Client* client, const std::vector<std::string> &cmd);
    std::string mouse_initiate_zoom(Client* client, const std::vector<std::string> &cmd);
    std::string mouse_initiate_resize(Client* client, const std::vector<std::string> &cmd);
    std::string mouse_call_command(Client* client, const std::vector<std::string> &cmd);
    std::string mouse_call_command_root_window(const std::vector<std::string> &cmd);

private:
    //! start dragging for the specified client (possibly up to some arguments), and return a error message
    //! if not possible
    using MouseFunction = std::string (MouseManager::*)(Client* client, const std::vector<std::string> &cmd);

    class MouseBinding {
    public:
        MouseCombo mousecombo;
        MouseFunction action = {};
        std::vector<std::string> cmd;
    };

    //! Currently defined mouse bindings (TODO: make this private as soon as possible)
    std::list<MouseBinding> binds;

    std::experimental::optional<MouseBinding> mouse_binding_find(unsigned int modifiers, unsigned int button);

    MouseFunction string2mousefunction(const std::string& name);

    //! manually (forward-)declare MouseDragHandler::Constructor as MDC here:
    typedef std::function<std::shared_ptr<MouseDragHandler>(MonitorManager*, Client*)> MDC;
    //! start a the drag, and if it does not work out, return an error message
    std::string mouse_initiate_drag(Client* client, const MDC& createHandler);

    std::map<std::string, MouseFunction> mouseFunctions_;
    std::shared_ptr<MouseDragHandler> dragHandler_;
    Cursor cursor;
    ClientManager*  clients_;
    MonitorManager*  monitors_;
};
