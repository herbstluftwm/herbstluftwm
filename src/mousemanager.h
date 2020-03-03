#pragma once

#include <X11/X.h>
#include <list>
#include <memory>

#include "mouse.h"
#include "object.h"
#include "optional.h"

class Completion;
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

    //! Currently defined mouse bindings (TODO: make this private as soon as possible)
    std::list<MouseBinding> binds;


    int mouse_unbind_all(Output o);
    std::experimental::optional<MouseBinding> mouse_binding_find(unsigned int modifiers, unsigned int button);

    MouseFunction string2mousefunction(const char* name);

    void grab_client_buttons(Client* client, bool focused);

    void mouse_handle_event(unsigned int modifiers, unsigned int button, Window window);
    void mouse_stop_drag();
    bool mouse_is_dragging();
    void handle_motion_event(Point2D newCursorPos);

    void mouse_initiate_move(Client* client, const std::vector<std::string> &cmd);
    void mouse_initiate_zoom(Client* client, const std::vector<std::string> &cmd);
    void mouse_initiate_resize(Client* client, const std::vector<std::string> &cmd);
    void mouse_call_command(Client* client, const std::vector<std::string> &cmd);

private:
    void mouse_initiate_drag(Client* client, void (MouseDragHandler::*function)(Point2D));

    std::shared_ptr<MouseDragHandler> dragHandler_;
    Cursor cursor;
    ClientManager*  clients_;
    MonitorManager*  monitors_;
};
