#include "xmainloop.h"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <memory>

#include "client.h"
#include "clientmanager.h"
#include "desktopwindow.h"
#include "ewmh.h"
#include "frametree.h"
#include "globals.h"
#include "hlwmcommon.h"
#include "ipc-server.h"
#include "keymanager.h"
#include "layout.h"
#include "monitor.h"
#include "mousemanager.h"
#include "root.h"
#include "settings.h"
#include "tag.h"
#include "utils.h"
#include "xconnection.h"

using std::shared_ptr;

XMainLoop::XMainLoop(XConnection& X, Root* root)
    : X_(X)
    , root_(root)
    , aboutToQuit_(false)
    , handlerTable_()
{
    handlerTable_[ ButtonPress       ] = &XMainLoop::buttonpress;
    handlerTable_[ ButtonRelease     ] = &XMainLoop::buttonrelease;
    handlerTable_[ ClientMessage     ] = &XMainLoop::clientmessage;
    handlerTable_[ ConfigureNotify   ] = &XMainLoop::configurenotify;
    handlerTable_[ ConfigureRequest  ] = &XMainLoop::configurerequest;
    handlerTable_[ CreateNotify      ] = &XMainLoop::createnotify;
    handlerTable_[ DestroyNotify     ] = &XMainLoop::destroynotify;
    handlerTable_[ EnterNotify       ] = &XMainLoop::enternotify;
    handlerTable_[ Expose            ] = &XMainLoop::expose;
    handlerTable_[ FocusIn           ] = &XMainLoop::focusin;
    handlerTable_[ KeyPress          ] = &XMainLoop::keypress;
    handlerTable_[ MapNotify         ] = &XMainLoop::mapnotify;
    handlerTable_[ MapRequest        ] = &XMainLoop::maprequest;
    handlerTable_[ MappingNotify     ] = &XMainLoop::mappingnotify;
    handlerTable_[ MotionNotify      ] = &XMainLoop::motionnotify;
    handlerTable_[ PropertyNotify    ] = &XMainLoop::propertynotify;
    handlerTable_[ UnmapNotify       ] = &XMainLoop::unmapnotify;
}

void XMainLoop::run() {
    XEvent event;
    int x11_fd;
    fd_set in_fds;
    x11_fd = ConnectionNumber(X_.display());
    while (!aboutToQuit_) {
        FD_ZERO(&in_fds);
        FD_SET(x11_fd, &in_fds);
        // wait for an event or a signal
        select(x11_fd + 1, &in_fds, nullptr, nullptr, nullptr);
        if (aboutToQuit_) {
            break;
        }
        XSync(X_.display(), False);
        while (XQLength(X_.display())) {
            XNextEvent(X_.display(), &event);
            EventHandler handler = handlerTable_[event.type];
            if (handler != nullptr) {
                (this ->* handler)(&event);
            }
            XSync(X_.display(), False);
        }
    }
}

void XMainLoop::quit() {
    aboutToQuit_ = true;
}

/* ----------------------------- */
/* event handler implementations */
/* ----------------------------- */

void XMainLoop::buttonpress(XEvent* event) {
    XButtonEvent* be = &(event->xbutton);
    MouseManager* mm = root_->mouse();
    HSDebug("name is: ButtonPress on sub %lx, win %lx\n", be->subwindow, be->window);
    if (mm->mouse_binding_find(be->state, be->button)) {
        mm->mouse_handle_event(be->state, be->button, be->window);
    } else {
        Client* client = root_->clients->client(be->window);
        if (client) {
            focus_client(client, false, true);
            if (root_->settings->raise_on_click()) {
                    client->raise();
            }
        }
    }
    XAllowEvents(g_display, ReplayPointer, be->time);
}

void XMainLoop::buttonrelease(XEvent*) {
    HSDebug("name is: ButtonRelease\n");
    root_->mouse->mouse_stop_drag();
}

void XMainLoop::createnotify(XEvent* event) {
    // printf("name is: CreateNotify\n");
    if (root_->ipcServer_.isConnectable(event->xcreatewindow.window)) {
        root_->ipcServer_.addConnection(event->xcreatewindow.window);
        root_->ipcServer_.handleConnection(event->xcreatewindow.window,
                                          HlwmCommon::callCommand);
    }
}

void XMainLoop::configurerequest(XEvent* event) {
    HSDebug("name is: ConfigureRequest\n");
    XConfigureRequestEvent* cre = &(event->xconfigurerequest);
    Client* client = root_->clients->client(cre->window);
    if (client) {
        bool changes = false;
        auto newRect = client->float_size_;
        if (client->sizehints_floating_ &&
            (client->is_client_floated() || client->pseudotile_))
        {
            bool width_requested = 0 != (cre->value_mask & CWWidth);
            bool height_requested = 0 != (cre->value_mask & CWHeight);
            bool x_requested = 0 != (cre->value_mask & CWX);
            bool y_requested = 0 != (cre->value_mask & CWY);
            cre->width += 2*cre->border_width;
            cre->height += 2*cre->border_width;
            if (width_requested && newRect.width  != cre->width) changes = true;
            if (height_requested && newRect.height != cre->height) changes = true;
            if (x_requested || y_requested) changes = true;
            if (x_requested) newRect.x = cre->x;
            if (y_requested) newRect.y = cre->y;
            if (width_requested) newRect.width = cre->width;
            if (height_requested) newRect.height = cre->height;
        }
        if (changes && client->is_client_floated()) {
            client->float_size_ = newRect;
            client->resize_floating(find_monitor_with_tag(client->tag()), client == get_current_client());
        } else if (changes && client->pseudotile_) {
            client->float_size_ = newRect;
            Monitor* m = find_monitor_with_tag(client->tag());
            if (m) m->applyLayout();
        } else {
        // FIXME: why send event and not XConfigureWindow or XMoveResizeWindow??
            client->send_configure();
        }
    } else {
        // if client not known.. then allow configure.
        // its probably a nice conky or dzen2 bar :)
        XWindowChanges wc;
        wc.x = cre->x;
        wc.y = cre->y;
        wc.width = cre->width;
        wc.height = cre->height;
        wc.border_width = cre->border_width;
        wc.sibling = cre->above;
        wc.stack_mode = cre->detail;
        XConfigureWindow(g_display, cre->window, cre->value_mask, &wc);
    }
}

void XMainLoop::clientmessage(XEvent* event) {
    root_->ewmh->handleClientMessage(&(event->xclient));
}

void XMainLoop::configurenotify(XEvent* event) {
    if (event->xconfigure.window == g_root &&
        root_->settings->auto_detect_monitors()) {
        const char* args[] = { "detect_monitors" };
        std::ostringstream void_output;
        detect_monitors_command(LENGTH(args), args, void_output);
    }
    // HSDebug("name is: ConfigureNotify\n");
}

void XMainLoop::destroynotify(XEvent* event) {
    // try to unmanage it
    //HSDebug("name is: DestroyNotify for %lx\n", event->xdestroywindow.window);
    auto cm = root_->clients();
    auto client = cm->client(event->xunmap.window);
    if (client) {
        cm->force_unmanage(client);
    } else {
        DesktopWindow::unregisterDesktop(event->xunmap.window);
    }
}

void XMainLoop::enternotify(XEvent* event) {
    XCrossingEvent *ce = &event->xcrossing;
    //HSDebug("name is: EnterNotify, focus = %d\n", event->xcrossing.focus);
    if (!root_->mouse->mouse_is_dragging()
        && root_->settings()->focus_follows_mouse()
        && ce->focus == false) {
        Client* c = root_->clients->client(ce->window);
        shared_ptr<HSFrameLeaf> target;
        if (c && c->tag()->floating == false
              && (target = c->tag()->frame->root_->frameWithClient(c))
              && target->getLayout() == LayoutAlgorithm::max
              && target->focusedClient() != c) {
            // don't allow focus_follows_mouse if another window would be
            // hidden during that focus change (which only occurs in max layout)
        } else if (c) {
            focus_client(c, false, true);
        }
    }
}

void XMainLoop::expose(XEvent* event) {
    //if (event->xexpose.count > 0) return;
    //Window ewin = event->xexpose.window;
    //HSDebug("name is: Expose for window %lx\n", ewin);
}

void XMainLoop::focusin(XEvent* event) {
    //HSDebug("name is: FocusIn\n");
}

void XMainLoop::keypress(XEvent* event) {
    //HSDebug("name is: KeyPress\n");
    root_->keys()->handleKeyPress(event);
}

void XMainLoop::mappingnotify(XEvent* event) {
    {
        // regrab when keyboard map changes
        XMappingEvent *ev = &event->xmapping;
        XRefreshKeyboardMapping(ev);
        if(ev->request == MappingKeyboard) {
            root_->keys()->regrabAll();
            //TODO: mouse_regrab_all();
        }
    }
}

void XMainLoop::motionnotify(XEvent* event) {
    if (event->type != MotionNotify) return;
    // get newest motion notification
    while (XCheckMaskEvent(g_display, ButtonMotionMask, event));
    Point2D newCursorPos = { event->xmotion.x_root,  event->xmotion.y_root };
    root_->mouse->handle_motion_event(newCursorPos);
}

void XMainLoop::mapnotify(XEvent* event) {
    //HSDebug("name is: MapNotify\n");
    Client* c = root_->clients()->client(event->xmap.window);
    if (c != nullptr) {
        // reset focus. so a new window gets the focus if it shall have the
        // input focus
        if (c == root_->clients->focus()) {
            XSetInputFocus(g_display, c->window_, RevertToPointerRoot, CurrentTime);
        }
        // also update the window title - just to be sure
        c->update_title();
    }
}

void XMainLoop::maprequest(XEvent* event) {
    HSDebug("name is: MapRequest\n");
    XMapRequestEvent* mapreq = &event->xmaprequest;
    Window window = mapreq->window;
    Client* c = root_->clients()->client(window);
    if (root_->ewmh->isOwnWindow(window)
        || is_herbstluft_window(g_display, window))
    {
        // just map the window if it wants that
        XWindowAttributes wa;
        if (!XGetWindowAttributes(g_display, window, &wa)) {
            return;
        }
        XMapWindow(g_display, window);
    } else if (c == nullptr) {
        if (root_->ewmh->getWindowType(window) == NetWmWindowTypeDesktop)
        {
            DesktopWindow::registerDesktop(window);
            DesktopWindow::lowerDesktopWindows();
            XMapWindow(g_display, window);
        } else {
            // client should be managed (is not ignored)
            // but is not managed yet
            auto clientmanager = root_->clients();
            auto client = clientmanager->manage_client(window, false);
            if (client && find_monitor_with_tag(client->tag())) {
                XMapWindow(g_display, window);
            }
        }
    }
    // else: ignore all other maprequests from windows
    // that are managed already
}

void XMainLoop::propertynotify(XEvent* event) {
    // printf("name is: PropertyNotify\n");
    XPropertyEvent *ev = &event->xproperty;
    Client* client = root_->clients->client(ev->window);
    if (ev->state == PropertyNewValue) {
        if (root_->ipcServer_.isConnectable(event->xproperty.window)) {
            root_->ipcServer_.handleConnection(event->xproperty.window,
                                              HlwmCommon::callCommand);
        } else if (client != nullptr) {
            if (ev->atom == XA_WM_HINTS) {
                client->update_wm_hints();
            } else if (ev->atom == XA_WM_NORMAL_HINTS) {
                client->updatesizehints();
                Monitor* m = find_monitor_with_tag(client->tag());
                if (m) m->applyLayout();
            } else if (ev->atom == XA_WM_NAME ||
                       ev->atom == g_netatom[NetWmName]) {
                client->update_title();
            }
        }
    }
}

void XMainLoop::unmapnotify(XEvent* event) {
    HSDebug("name is: UnmapNotify for %lx\n", event->xunmap.window);
    root_->clients()->unmap_notify(event->xunmap.window);
}

