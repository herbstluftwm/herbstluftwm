import conftest
import os
import pytest


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


@pytest.mark.parametrize('utf8names,desktop_names', [
    (False, ['default']),
    (False, ['foo']),
    (True, ['föö', 'bär']),
    (True, ['a', 'long', 'list', 'of', 'tag', 'names']),
    (True, ['an', 'empty', '', '', 'tagname']),
])
def test_read_desktop_names(hlwm_spawner, x11, utf8names, desktop_names):
    x11.set_property_textlist('_NET_DESKTOP_NAMES', desktop_names, utf8=utf8names)
    x11.display.sync()

    hlwm_proc = hlwm_spawner()
    hlwm = conftest.HlwmBridge(os.environ['DISPLAY'], hlwm_proc)

    desktop_names = [dn for dn in desktop_names if len(dn) > 0]

    assert hlwm.list_children('tags.by-name') == sorted(desktop_names)
    for idx, name in enumerate(desktop_names):
        assert hlwm.get_attr(f'tags.{idx}.name') == name

    hlwm_proc.shutdown()


@pytest.mark.parametrize('desktops,client2desktop', [
    (2, [0, 1]),
    (2, [None, 1]),  # client without index set
    (2, [2, 1, 8, 0]),  # clients exceeding the index range
    (5, [1, 1, 0, 4, 4, 4]),
])
def test_client_initially_on_desktop(hlwm_spawner, x11, desktops, client2desktop):
    desktop_names = ['tag{}'.format(i) for i in range(0, desktops)]
    x11.set_property_textlist('_NET_DESKTOP_NAMES', desktop_names)
    clients = []
    for desktop_idx in client2desktop:
        handle, winid = x11.create_client(sync_hlwm=False)
        clients.append(winid)
        if desktop_idx is not None:
            x11.set_property_cardinal('_NET_WM_DESKTOP', [desktop_idx], window=handle)
    x11.display.sync()

    hlwm_proc = hlwm_spawner()
    hlwm = conftest.HlwmBridge(os.environ['DISPLAY'], hlwm_proc)

    for client_idx, desktop_idx in enumerate(client2desktop):
        winid = clients[client_idx]
        if desktop_idx is not None and desktop_idx in range(0, desktops):
            assert hlwm.get_attr(f'clients.{winid}.tag') \
                == desktop_names[desktop_idx]
        else:
            assert hlwm.get_attr(f'clients.{winid}.tag') \
                == desktop_names[0]

    hlwm_proc.shutdown()
