def test_net_wm_desktop_after_load(hlwm, x11):
    hlwm.call('add anothertag')
    win, _ = x11.create_client()
    hlwm.call('true')
    assert x11.get_property('_NET_WM_DESKTOP', win)[0] == 0
    layout = hlwm.call('dump').stdout

    hlwm.call(['load', 'anothertag', layout])

    assert x11.get_property('_NET_WM_DESKTOP', win)[0] == 1


def test_net_wm_desktop_after_bring(hlwm, x11):
    hlwm.call('add anothertag')
    win, winid = x11.create_client()
    hlwm.call('true')
    assert x11.get_property('_NET_WM_DESKTOP', win)[0] == 0

    hlwm.call(['use', 'anothertag'])
    hlwm.call(['bring', winid])

    assert x11.get_property('_NET_WM_DESKTOP', win)[0] == 1
