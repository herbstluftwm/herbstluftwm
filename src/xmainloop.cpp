#include "xmainloop.h"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <iostream>
#include <memory>

#include "client.h"
#include "clientmanager.h"
#include "decoration.h"
#include "desktopwindow.h"
#include "ewmh.h"
#include "framedecoration.h"
#include "frametree.h"
#include "hlwmcommon.h"
#include "ipc-server.h"
#include "keymanager.h"
#include "layout.h"
#include "monitor.h"
#include "monitormanager.h"
#include "mousemanager.h"
#include "panelmanager.h"
#include "root.h"
#include "rules.h"
#include "settings.h"
#include "tag.h"
#include "tagmanager.h"
#include "utils.h"
#include "xconnection.h"

using std::function;
using std::shared_ptr;

/** A custom event handler casting function.
 *
 * It ensures that the pointer that is casted to the EventHandler
 * type is indeed a member function that accepts one pointer.
 *
 * Note that this is as (much or less) hacky as a member access to the XEvent
 * union type itself.
 */
template<typename T>
inline XMainLoop::EventHandler EH(void (XMainLoop::*handler)(T*)) {
    return (XMainLoop::EventHandler) handler;
}

XMainLoop::XMainLoop(XConnection& X, Root* root)
    : X_(X)
    , root_(root)
    , aboutToQuit_(false)
    , handlerTable_()
{
    handlerTable_[ ButtonPress       ] = EH(&XMainLoop::buttonpress);
    handlerTable_[ ButtonRelease     ] = EH(&XMainLoop::buttonrelease);
    handlerTable_[ ClientMessage     ] = EH(&XMainLoop::clientmessage);
    handlerTable_[ ConfigureNotify   ] = EH(&XMainLoop::configurenotify);
    handlerTable_[ ConfigureRequest  ] = EH(&XMainLoop::configurerequest);
    handlerTable_[ CreateNotify      ] = EH(&XMainLoop::createnotify);
    handlerTable_[ DestroyNotify     ] = EH(&XMainLoop::destroynotify);
    handlerTable_[ EnterNotify       ] = EH(&XMainLoop::enternotify);
    handlerTable_[ Expose            ] = EH(&XMainLoop::expose);
    handlerTable_[ FocusIn           ] = EH(&XMainLoop::focusin);
    handlerTable_[ KeyPress          ] = EH(&XMainLoop::keypress);
    handlerTable_[ MapNotify         ] = EH(&XMainLoop::mapnotify);
    handlerTable_[ MapRequest        ] = EH(&XMainLoop::maprequest);
    handlerTable_[ MappingNotify     ] = EH(&XMainLoop::mappingnotify);
    handlerTable_[ MotionNotify      ] = EH(&XMainLoop::motionnotify);
    handlerTable_[ PropertyNotify    ] = EH(&XMainLoop::propertynotify);
    handlerTable_[ UnmapNotify       ] = EH(&XMainLoop::unmapnotify);

    root_->monitors->dropEnterNotifyEvents
            .connect(this, &XMainLoop::dropEnterNotifyEvents);
}

//! scan for windows and add them to the list of managed clients
// from dwm.c
void XMainLoop::scanExistingClients() {
    XWindowAttributes wa;
    auto clientmanager = root_->clients();
    auto& initialEwmhState = root_->ewmh->initialState();
    auto& originalClients = initialEwmhState.original_client_list_;
    auto isInOriginalClients = [&originalClients] (Window win) {
        return originalClients.end()
            != std::find(originalClients.begin(), originalClients.end(), win);
    };
    auto findTagForWindow = [this](Window win) -> function<void(ClientChanges&)> {
            if (!root_->globals.importTagsFromEwmh) {
                // do nothing, if import is disabled
                return [] (ClientChanges&) {};
            }
            return [this,win] (ClientChanges& changes) {
                long idx = this->root_->ewmh->windowGetInitialDesktop(win);
                if (idx < 0) {
                    return;
                }
                HSTag* tag = root_->tags->byIdx((size_t)idx);
                if (tag) {
                    changes.tag_name = tag->name();
                }
            };
    };
    for (auto win : X_.queryTree(X_.root())) {
        if (!XGetWindowAttributes(X_.display(), win, &wa) || wa.override_redirect)
        {
            continue;
        }
        // only manage mapped windows.. no strange wins like:
        //      luakit/dbus/(ncurses-)vim
        // but manage it if it was in the ewmh property _NET_CLIENT_LIST by
        // the previous window manager
        // TODO: what would dwm do?
        if (root_->ewmh->isOwnWindow(win)) {
            continue;
        }
        if (root_->ewmh->getWindowType(win) == NetWmWindowTypeDesktop)
        {
            DesktopWindow::registerDesktop(win);
            DesktopWindow::lowerDesktopWindows();
            XMapWindow(X_.display(), win);
        }
        else if (root_->ewmh->getWindowType(win) == NetWmWindowTypeDock)
        {
            root_->panels->registerPanel(win);
            XSelectInput(X_.display(), win, PropertyChangeMask);
            XMapWindow(X_.display(), win);
        }
        else if (wa.map_state == IsViewable
            || isInOriginalClients(win)) {
            Client* c = clientmanager->manage_client(win, true, false, findTagForWindow(win));
            if (root_->monitors->byTag(c->tag())) {
                XMapWindow(X_.display(), win);
            }
        }
    }
    // ensure every original client is managed again
    for (auto win : originalClients) {
        if (clientmanager->client(win)) {
            continue;
        }
        if (!XGetWindowAttributes(X_.display(), win, &wa)
            || wa.override_redirect)
        {
            continue;
        }
        XReparentWindow(X_.display(), win, X_.root(), 0,0);
        clientmanager->manage_client(win, true, false, findTagForWindow(win));
    }
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

void XMainLoop::dropEnterNotifyEvents()
{
    if (duringEnterNotify_) {
        // during a enternotify(), no artificial enter notify events
        // can be created. Moreover, on quick mouse movements, an enter notify
        // might be followed by further enter notify events, which
        // must not be dropped.
        return;
    }
    XEvent ev;
    XSync(X_.display(), False);
    while (XCheckMaskEvent(X_.display(), EnterWindowMask, &ev)) {
    }
}

/* ----------------------------- */
/* event handler implementations */
/* ----------------------------- */

void XMainLoop::buttonpress(XButtonEvent* be) {
    MouseManager* mm = root_->mouse();
    HSDebug("name is: ButtonPress on sub 0x%lx, win 0x%lx\n", be->subwindow, be->window);
    if (!mm->mouse_handle_event(be->state, be->button, be->window)) {
        // if the event was not handled by the mouse manager, pass it to the client:
        Client* client = root_->clients->client(be->window);
        if (!client) {
            client = Decoration::toClient(be->window);
        }
        if (client) {
            bool raise = root_->settings->raise_on_click();
            focus_client(client, false, true, raise);
            if (be->window == client->decorationWindow()) {
                if (client->dec->positionTriggersResize({be->x, be->y})) {
                    mm->mouse_initiate_resize(client, {});
                } else {
                    mm->mouse_initiate_move(client, {});
                }
            }
        } else {
            // root windows handling
            HSDebug("handle default RootWindow ButtonPress on win 0x%lx\n", be->window);
            mm->mouse_call_command_root_window({"call", "spwan","xmenu.sh"});
        }
    }
    FrameDecoration* frameDec = FrameDecoration::withWindow(be->window);
    if (frameDec) {
        auto frame = frameDec->frame();
        if (frame)  {
            root_->focusFrame(frame);
        }
    }
    XAllowEvents(X_.display(), ReplayPointer, be->time);
}

void XMainLoop::buttonrelease(XButtonEvent*) {
    HSDebug("name is: ButtonRelease\n");
    root_->mouse->mouse_stop_drag();
}

void XMainLoop::createnotify(XCreateWindowEvent* event) {
    // printf("name is: CreateNotify\n");
    if (root_->ipcServer_.isConnectable(event->window)) {
        root_->ipcServer_.addConnection(event->window);
        root_->ipcServer_.handleConnection(event->window,
                                           HlwmCommon::callCommand);
    }
}

void XMainLoop::configurerequest(XConfigureRequestEvent* cre) {
    HSDebug("name is: ConfigureRequest for 0x%lx\n", cre->window);
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
            if (width_requested && newRect.width != cre->width) {
                changes = true;
            }
            if (height_requested && newRect.height != cre->height) {
                changes = true;
            }
            if (x_requested || y_requested) {
                changes = true;
            }
            if (x_requested || y_requested) {
                // if only one of the two dimensions is requested, then just
                // set the other to some reasonable value.
                if (!x_requested) {
                    cre->x = client->last_size_.x;
                }
                if (!y_requested) {
                    cre->y = client->last_size_.y;
                }
                // interpret the x and y coordinate relative to the monitor they are currently on
                Monitor* m = root_->monitors->byTag(client->tag());
                if (!m) {
                    // if the client is not visible at the moment, take the monitor that is
                    // most appropriate according to the requested cooridnates:
                    m = root_->monitors->byCoordinate({cre->x, cre->y});
                }
                if (!m) {
                    // if we have not found a suitable monitor, take the current
                    m = root_->monitors->focus();
                }
                // the requested coordinates are relative to the root window.
                // convert them to coordinates relative to the monitor.
                cre->x -= m->rect.x + *m->pad_left;
                cre->y -= m->rect.y + *m->pad_up;
                newRect.x = cre->x;
                newRect.y = cre->y;
            }
            if (width_requested) {
                newRect.width = cre->width;
            }
            if (height_requested) {
                newRect.height = cre->height;
            }
        }
        if (changes && client->is_client_floated()) {
            client->float_size_ = newRect;
            client->resize_floating(find_monitor_with_tag(client->tag()), client == get_current_client());
        } else if (changes && client->pseudotile_) {
            client->float_size_ = newRect;
            Monitor* m = find_monitor_with_tag(client->tag());
            if (m) {
                m->applyLayout();
            }
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
        XConfigureWindow(X_.display(), cre->window, cre->value_mask, &wc);
    }
}

void XMainLoop::clientmessage(XClientMessageEvent* event) {
    root_->ewmh->handleClientMessage(event);
}

void XMainLoop::configurenotify(XConfigureEvent* event) {
    if (event->window == g_root) {
        root_->panels->rootWindowChanged(event->width, event->height);
        if (root_->settings->auto_detect_monitors()) {
            Input input = Input("detect_monitors");
            std::ostringstream void_output;
            root_->monitors->detectMonitorsCommand(input, void_output);
        }
    }
    // HSDebug("name is: ConfigureNotify\n");
}

void XMainLoop::destroynotify(XUnmapEvent* event) {
    // try to unmanage it
    //HSDebug("name is: DestroyNotify for %lx\n", event->xdestroywindow.window);
    auto cm = root_->clients();
    auto client = cm->client(event->window);
    if (client) {
        cm->force_unmanage(client);
    } else {
        DesktopWindow::unregisterDesktop(event->window);
        root_->panels->unregisterPanel(event->window);
    }
}

void XMainLoop::enternotify(XCrossingEvent* ce) {
    HSDebug("name is: EnterNotify, focus = %d, window = 0x%lx\n", ce->focus, ce->window);
    if (ce->mode != NotifyNormal || ce->detail == NotifyInferior) {
        // ignore an event if it is caused by (un-)grabbing the mouse or
        // if the pointer moves from a window to its decoration.
        // for 'ce->detail' see:
        // https://tronche.com/gui/x/xlib/events/window-entry-exit/normal.html
        return;
    }
    // Warning: we have to set this to false again!
    duringEnterNotify_ = true;
    if (!root_->mouse->mouse_is_dragging()
        && root_->settings()->focus_follows_mouse()
        && ce->focus == false) {
        Client* c = root_->clients->client(ce->window);
        if (!c) {
            c = Decoration::toClient(ce->window);
        }
        shared_ptr<FrameLeaf> target;
        if (c && c->tag()->floating == false
              && (target = c->tag()->frame->root_->frameWithClient(c))
              && target->getLayout() == LayoutAlgorithm::max
              && target->focusedClient() != c) {
            // don't allow focus_follows_mouse if another window would be
            // hidden during that focus change (which only occurs in max layout)
        } else if (c) {
            focus_client(c, false, true, false);
        }
        if (!c) {
            // if it's not a client window, it's maybe a frame
            FrameDecoration* frameDec = FrameDecoration::withWindow(ce->window);
            if (frameDec) {
                auto frame = frameDec->frame();
                HSWeakAssert(frame);
                if (frame) {
                    root_->focusFrame(frame);
                }
            }
        }
    }
    duringEnterNotify_ = false;
}

void XMainLoop::expose(XEvent* event) {
    //if (event->xexpose.count > 0) return;
    //Window ewin = event->xexpose.window;
    //HSDebug("name is: Expose for window %lx\n", ewin);
}

void XMainLoop::focusin(XEvent* event) {
    //HSDebug("name is: FocusIn\n");
}

void XMainLoop::keypress(XKeyEvent* event) {
    //HSDebug("name is: KeyPress\n");
    root_->keys()->handleKeyPress(event);
}

void XMainLoop::mappingnotify(XMappingEvent* ev) {
    // regrab when keyboard map changes
    XRefreshKeyboardMapping(ev);
    if(ev->request == MappingKeyboard) {
        root_->keys()->regrabAll();
        //TODO: mouse_regrab_all();
    }
}

void XMainLoop::motionnotify(XMotionEvent* event) {
    // get newest motion notification
    while (XCheckMaskEvent(X_.display(), ButtonMotionMask, (XEvent *)event)) {
        ;
    }
    Point2D newCursorPos = { event->x_root,  event->y_root };
    root_->mouse->handle_motion_event(newCursorPos);
}

void XMainLoop::mapnotify(XMapEvent* event) {
    //HSDebug("name is: MapNotify\n");
    Client* c = root_->clients()->client(event->window);
    if (c != nullptr) {
        // reset focus. so a new window gets the focus if it shall have the
        // input focus
        if (c == root_->clients->focus()) {
            XSetInputFocus(X_.display(), c->window_, RevertToPointerRoot, CurrentTime);
        }
        // also update the window title - just to be sure
        c->update_title();
    } else if (!root_->ewmh->isOwnWindow(event->window)
               && !is_herbstluft_window(X_.display(), event->window)) {
        // the window is not managed.
        HSDebug("MapNotify: briefly managing 0x%lx to apply rules\n", event->window);
        root_->clients()->manage_client(event->window, true, true);
    }
}

void XMainLoop::maprequest(XMapRequestEvent* mapreq) {
    HSDebug("name is: MapRequest for 0x%lx\n", mapreq->window);
    Window window = mapreq->window;
    Client* c = root_->clients()->client(window);
    if (root_->ewmh->isOwnWindow(window)
        || is_herbstluft_window(X_.display(), window))
    {
        // just map the window if it wants that
        XWindowAttributes wa;
        if (!XGetWindowAttributes(X_.display(), window, &wa)) {
            return;
        }
        XMapWindow(X_.display(), window);
    } else if (c == nullptr) {
        if (root_->ewmh->getWindowType(window) == NetWmWindowTypeDesktop)
        {
            DesktopWindow::registerDesktop(window);
            DesktopWindow::lowerDesktopWindows();
            XMapWindow(X_.display(), window);
        }
        else if (root_->ewmh->getWindowType(window) == NetWmWindowTypeDock)
        {
            root_->panels->registerPanel(window);
            XSelectInput(X_.display(), window, PropertyChangeMask);
            XMapWindow(X_.display(), window);
        } else {
            // client should be managed (is not ignored)
            // but is not managed yet
            auto clientmanager = root_->clients();
            auto client = clientmanager->manage_client(window, false, false);
            if (client && find_monitor_with_tag(client->tag())) {
                XMapWindow(X_.display(), window);
            }
        }
    }
    // else: ignore all other maprequests from windows
    // that are managed already
}

void XMainLoop::propertynotify(XPropertyEvent* ev) {
    // printf("name is: PropertyNotify\n");
    Client* client = root_->clients->client(ev->window);
    if (ev->state == PropertyNewValue) {
        if (root_->ipcServer_.isConnectable(ev->window)) {
            root_->ipcServer_.handleConnection(ev->window,
                                               HlwmCommon::callCommand);
        } else if (client != nullptr) {
            //char* atomname = XGetAtomName(X_.display(), ev->atom);
            //HSDebug("Property notify for client %s: atom %d \"%s\"\n",
            //        client->window_id_str().c_str(),
            //        ev->atom,
            //        atomname);
            if (ev->atom == XA_WM_HINTS) {
                client->update_wm_hints();
            } else if (ev->atom == XA_WM_NORMAL_HINTS) {
                client->updatesizehints();
                Monitor* m = find_monitor_with_tag(client->tag());
                if (m) {
                    m->applyLayout();
                }
            } else if (ev->atom == XA_WM_NAME ||
                       ev->atom == root_->ewmh->netatom(NetWmName)) {
                client->update_title();
            } else if (ev->atom == XA_WM_CLASS && client) {
                // according to the ICCCM specification, the WM_CLASS property may only
                // be changed in the withdrawn state:
                // https://www.x.org/releases/X11R7.6/doc/xorg-docs/specs/ICCCM/icccm.html#wm_class_property
                // If a client violates this, then the window rules like class=... etc are not applied.
                // As a workaround, we do it now:
                root_->clients()->applyRules(client, std::cerr);
            }
        } else {
            root_->panels->propertyChanged(ev->window, ev->atom);
        }
    }
}

void XMainLoop::unmapnotify(XUnmapEvent* event) {
    HSDebug("name is: UnmapNotify for %lx\n", event->window);
    root_->clients()->unmap_notify(event->window);
    if (event->send_event) {
        // if the event was synthetic, then we need to understand it as a kind request
        // by the window to be unmanaged. I don't understand fully how this is implied
        // by the ICCCM documentation:
        // https://tronche.com/gui/x/icccm/sec-4.html#s-4.1.4
        //
        // Anyway, we need to do the following because when running
        // "telegram-desktop -startintray", a window flashes and only
        // sends a synthetic UnmapNotify. So we unmanage the window here
        // to forcefully make the window dissappear.
        XUnmapWindow(X_.display(), event->window);
    }
    // drop all enternotify events
    XSync(X_.display(), False);
    XEvent ev;
    while (XCheckMaskEvent(X_.display(), EnterWindowMask, &ev)) {
        ;
    }
}

