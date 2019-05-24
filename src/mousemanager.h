#pragma once

#include <X11/X.h>
#include <list>

#include "mouse.h"
#include "object.h"

class Completion;

typedef void (MouseManager::*MouseDragFunction)(XMotionEvent*);

class MouseManager : public Object {
public:
    MouseManager();
    ~MouseManager();

    int addMouseBindCommand(Input input, Output output);

    void addMouseBindCompletion(Completion &complete);

    //! Currently defined mouse bindings (TODO: make this private as soon as possible)
    std::list<MouseBinding> binds;


    int mouse_unbind_all(Output o);
    std::experimental::optional<MouseBinding> mouse_binding_find(unsigned int modifiers, unsigned int button);

    unsigned int string2button(const char* name);
    MouseFunction string2mousefunction(const char* name);

    void grab_client_buttons(Client* client, bool focused);

    void mouse_handle_event(XEvent* ev);
    void mouse_initiate_drag(Client* client, MouseDragFunction function);
    void mouse_stop_drag();
    bool mouse_is_dragging();
    void handle_motion_event(XEvent* ev);

    void mouse_initiate_move(Client* client, const std::vector<std::string> &cmd);
    void mouse_initiate_zoom(Client* client, const std::vector<std::string> &cmd);
    void mouse_initiate_resize(Client* client, const std::vector<std::string> &cmd);
    void mouse_call_command(Client* client, const std::vector<std::string> &cmd);
    /* some mouse drag functions */
    void mouse_function_move(XMotionEvent* me);
    void mouse_function_resize(XMotionEvent* me);
    void mouse_function_zoom(XMotionEvent* me);

private:
    Cursor cursor;
};
