import pytest
from Xlib import X


@pytest.mark.parametrize("running_clients_num", [0, 1, 2])
def test_window_focus(hlwm, x11, hc_idle, running_clients_num):
    # before creating the clients, assert that new clients are focused
    hlwm.call('rule focus=on')

    # create some clients:
    last_client = None
    for k in range(running_clients_num):
        last_client, _ = x11.create_client()

    hlwm.call('true')  # sync with hlwm
    x11.display.sync()

    assert x11.ewmh.getActiveWindow() == last_client
    if last_client is None:
        # if no window is focused, some dummy window is focused. in the case of hlwm
        # we focus the window that is also used for the EWMH _NET_SUPPORTING_WM_CHECK.
        # (this is also what openbox is doing)
        assert x11.display.get_input_focus().focus.id == x11.get_property('_NET_SUPPORTING_WM_CHECK')[0]
        assert 'focus' not in hlwm.list_children('clients')
    else:
        hlwm.call('attr clients')
        print(hc_idle.hooks())
        print(x11.display.get_input_focus().focus)
        assert x11.display.get_input_focus().focus == last_client
        assert x11.winid_str(last_client) == hlwm.get_attr('clients.focus.winid')


def test_client_moveresizes_itself(hlwm, x11):
    # create a floating window
    hlwm.call('move_monitor 0 500x600+12+13 14 15 16 17')
    hlwm.call('floating on')
    hlwm.call('set_attr theme.border_width 0')
    # FIXME: why doesn't this work with sync_hlwm=True?
    w, _ = x11.create_client(geometry=(25, 26, 27, 28), sync_hlwm=False)

    # resize the window to some other geometry
    w.configure(x=60, y=70, width=300, height=200)
    x11.display.sync()

    hlwm.call('true')  # sync with hlwm
    # check that w now has the desired geometry
    win_geo = w.get_geometry()
    assert (win_geo.width, win_geo.height) == (300, 200)
    x, y = x11.get_absolute_top_left(w)
    assert (x, y) == (60, 70)


def test_focus_steal_via_xsetinputfocus(hlwm, x11):
    oldfocus, old_id = x11.create_client()
    newfocus, new_id = x11.create_client()

    hlwm.call(['jumpto', old_id])
    assert hlwm.attr.clients.focus.winid() == old_id

    # steal the focus like XSetInputFocus in `xdotool windowfocus` would do it:
    x11.display.set_input_focus(newfocus, X.RevertToParent, X.CurrentTime)
    x11.display.sync()

    assert hlwm.attr.clients.focus.winid() == new_id
