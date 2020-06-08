import conftest
import os
import pytest
from Xlib import X


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


def test_tags_restored_after_wmexec(hlwm, hlwm_process):
    tags = ['a', 'long', 'list', 'of', 'tag', 'names']
    expected_tags = ['default'] + tags

    for tag in tags:
        hlwm.call(['add', tag])

    # We need at least one client, otherwise xvfb messes with the test
    hlwm.create_client()

    # Restart hlwm:
    hlwm.call(['wmexec', hlwm_process.bin_path, '--verbose'])
    hlwm_process.read_and_echo_output(until_stdout='hlwm started')

    assert hlwm.list_children('tags.by-name') == sorted(expected_tags)
    for idx, name in enumerate(expected_tags):
        assert hlwm.get_attr(f'tags.{idx}.name') == name


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
        winHandle, winid = x11.create_client(sync_hlwm=False)
        clients.append(winid)
        if desktop_idx is not None:
            x11.set_property_cardinal('_NET_WM_DESKTOP', [desktop_idx], window=winHandle)
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


def test_manage_transient_for_windows_on_startup(hlwm_spawner, x11):
    master_win, master_id = x11.create_client(sync_hlwm=False)
    dialog_win, dialog_id = x11.create_client(sync_hlwm=False)
    dialog_win.set_wm_transient_for(master_win)
    x11.display.sync()

    hlwm_proc = hlwm_spawner()
    hlwm = conftest.HlwmBridge(os.environ['DISPLAY'], hlwm_proc)

    assert hlwm.list_children('clients') \
        == sorted([master_id, dialog_id, 'focus'])
    hlwm_proc.shutdown()


@pytest.mark.parametrize('swap_monitors_to_get_tag', [True, False])
@pytest.mark.parametrize('on_another_monitor', [True, False])
@pytest.mark.parametrize('tag_idx', [0, 1])
def test_ewmh_set_current_desktop(hlwm, x11, swap_monitors_to_get_tag, on_another_monitor, tag_idx):
    hlwm.call(['set', 'swap_monitors_to_get_tag', hlwm.bool(swap_monitors_to_get_tag)])
    hlwm.call('add otherTag')
    hlwm.call('add anotherTag')

    if on_another_monitor:
        hlwm.call('add_monitor 800x600+800+0 otherTag')

    x11.ewmh.setCurrentDesktop(tag_idx)
    x11.display.sync()

    assert int(hlwm.get_attr('tags.focus.index')) == tag_idx
    if swap_monitors_to_get_tag or not on_another_monitor or tag_idx == 0:
        assert int(hlwm.get_attr('monitors.focus.index')) == 0
    else:
        assert int(hlwm.get_attr('monitors.focus.index')) == 1


def test_ewmh_set_current_desktop_invalid_idx(hlwm, hlwm_process, x11):
    with hlwm_process.wait_stderr_match('_NET_CURRENT_DESKTOP: invalid index'):
        x11.ewmh.setCurrentDesktop(4)
        x11.display.sync()
    assert int(hlwm.get_attr('tags.focus.index')) == 0


def test_wm_state_type(hlwm, x11):
    win, _ = x11.create_client(sync_hlwm=True)
    wm_state = x11.display.intern_atom('WM_STATE')
    prop = win.get_full_property(wm_state, X.AnyPropertyType)
    assert prop.property_type == wm_state
    assert len(prop.value) == 2


def test_ewmh_focus_client(hlwm, x11):
    hlwm.call('set focus_stealing_prevention off')
    # add another client that has the focus
    _, winid_focus = x11.create_client()
    winHandleToBeFocused, winid = x11.create_client()
    assert hlwm.get_attr('clients.focus.winid') == winid_focus

    x11.ewmh.setActiveWindow(winHandleToBeFocused)
    x11.display.flush()

    assert hlwm.get_attr('clients.focus.winid') == winid


@pytest.mark.parametrize('on_another_monitor', [True, False])
def test_ewmh_focus_client_on_other_tag(hlwm, x11, on_another_monitor):
    hlwm.call('set focus_stealing_prevention off')
    hlwm.call('add tag2')
    if on_another_monitor:  # if the tag shall be on another monitor
        hlwm.call('add_monitor 800x600+600+0')
    hlwm.call('rule tag=tag2 focus=off')
    # add another client that has the focus on the other tag
    x11.create_client()
    handle, winid = x11.create_client()
    assert 'focus' not in hlwm.list_children('clients')

    x11.ewmh.setActiveWindow(handle)
    x11.display.flush()

    assert hlwm.get_attr('tags.focus.name') == 'tag2'
    assert hlwm.get_attr('clients.focus.winid') == winid


def test_ewmh_move_client_to_tag(hlwm, x11):
    hlwm.call('set focus_stealing_prevention off')
    hlwm.call('add otherTag')
    winHandleToMove, winid = x11.create_client()
    assert hlwm.get_attr(f'clients.{winid}.tag') == 'default'

    x11.ewmh.setWmDesktop(winHandleToMove, 1)
    x11.display.sync()

    assert hlwm.get_attr(f'clients.{winid}.tag') == 'otherTag'


def test_ewmh_make_client_urgent(hlwm, hc_idle, x11):
    hlwm.call('set focus_stealing_prevention off')
    hlwm.call('add otherTag')
    hlwm.call('rule tag=otherTag')
    # create a new client that is not focused
    winHandle, winid = x11.create_client()
    assert hlwm.get_attr(f'clients.{winid}.urgent') == 'false'
    assert 'focus' not in hlwm.list_children('clients')
    # assert that this window really does not have wm hints set:
    assert winHandle.get_wm_hints() is None
    hc_idle.hooks()  # reset hooks

    demandsAttent = '_NET_WM_STATE_DEMANDS_ATTENTION'
    x11.ewmh.setWmState(winHandle, 1, demandsAttent)
    x11.display.flush()

    assert hlwm.get_attr(f'clients.{winid}.urgent') == 'true'
    assert ['tag_flags'] in hc_idle.hooks()
