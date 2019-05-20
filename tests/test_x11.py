import pytest


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
