import re
import pytest
from Xlib import Xatom


def strip_winids(string):
    """
    Replaces all substrings that look like window IDs with a fixed string.
    """
    return re.sub(r'0x([0-9a-f]+)', '<windowid>', string)


def helper_get_stack_as_list(hlwm, clients_only=True, strip_focus_layer=False):
    stack_stdout = hlwm.call('stack').stdout
    if strip_focus_layer:
        # remove all lines after the "Focus-Layer"-line
        # that contain the word "Client". Note that . does not
        # match newline.
        stack_stdout = re.sub('Focus-Layer(\n.*Client.*)*',
                              'Focus-Layer',
                              stack_stdout,
                              flags=re.MULTILINE)
    if clients_only:
        matches = re.finditer('Client (0x[0-9a-f]+)', stack_stdout)
        winids = [m.group(1) for m in matches]
    else:
        # extract all window IDs out of the stack command
        winids = re.findall(r'0x[0-9a-f]+', stack_stdout)
    # remove duplicates
    winids_new = []
    for w in winids:
        if w not in winids_new:
            winids_new.append(w)
    return winids_new


@pytest.mark.parametrize('count', [2, 5])
def test_clients_stacked_in_reverse_order_of_creation(hlwm, count):
    hlwm.call('floating on')

    clients = hlwm.create_clients(count)

    clients.reverse()
    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True) == clients


@pytest.mark.parametrize('floatingmode', ['on', 'off'])
def test_raise_client_already_on_top(hlwm, floatingmode):
    hlwm.call(['floating', floatingmode])
    if floatingmode == 'off':
        # in tiling mode, the focused client
        # is raise, so make sure c2 is focused
        hlwm.call(['rule', 'focus=on'])
    c1, _ = hlwm.create_client()
    c2, _ = hlwm.create_client()

    hlwm.call(['raise', c2])

    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True) == [c2, c1]


def test_focused_tiling_client_stays_on_top_in_max_layout(hlwm):
    # in tiling mode, the focused window is always the top window within the
    # tiling layer. Hence, trying to lower the focused window or to raise any
    # other window must be undone immediately.
    #
    # the same holds for the 'selected' window within a max layout
    # hlwm.attr.settings.hide_covered_windows = 'off'
    win = hlwm.create_clients(7)
    layout = f"""
    (split horizontal:0.5:0
      (clients max:0 {win[0]} {win[1]})
      (split vertical:0.5:0
        (clients max:1 {win[2]} {win[3]} {win[4]})
        (clients max:0 {win[5]} {win[6]})))
    """
    hlwm.call(['load', layout])

    def verify_stack():
        """verify that the selected window in each max
        layout frame is above all other windows of the same
        frame
        """
        stack = helper_get_stack_as_list(hlwm)
        assert stack.index(win[0]) < stack.index(win[1])
        assert stack.index(win[3]) < stack.index(win[2])
        assert stack.index(win[3]) < stack.index(win[4])
        assert stack.index(win[5]) < stack.index(win[6])

    for command in ['raise', 'lower']:
        for winid in win:
            hlwm.call([command, winid])
            verify_stack()


def test_raise_bottom_client(hlwm):
    hlwm.call('floating on')
    c1, c2 = hlwm.create_clients(2)

    hlwm.call(['raise', c1])

    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True) == [c1, c2]


def test_lower_topmost_client(hlwm):
    hlwm.call('floating on')
    hlwm.call('rule floating=on')
    # new clients are placed on top, so put clients
    # into stacking order by reversing the list:
    clients = list(reversed(hlwm.create_clients(4)))
    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True) == clients

    hlwm.call(['lower', clients[0]])

    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True) == clients[1:] + [clients[0]]


def test_initial_net_wm_state_above_stays_above_later_clients(hlwm, x11):
    above_atom = x11.display.intern_atom('_NET_WM_STATE_ABOVE')
    state_atom = x11.display.intern_atom('_NET_WM_STATE')

    def request_above_before_map(window):
        window.change_property(state_atom, Xatom.ATOM, 32, [above_atom])

    _, above = x11.create_client(pre_map=request_above_before_map)
    _, ordinary = x11.create_client()

    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True)[:2] \
        == [above, ordinary]
    assert '_NET_WM_STATE_ABOVE' in \
        x11.ewmh.getWmState(x11.window(above), str=True)


def test_net_wm_state_above_client_messages_control_stacking(hlwm, x11):
    hlwm.call('floating on')
    above_window, above = x11.create_client()
    _, ordinary = x11.create_client()
    above_atom = '_NET_WM_STATE_ABOVE'

    x11.ewmh.setWmState(above_window, 1, above_atom)
    x11.sync_with_hlwm()
    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True)[:2] \
        == [above, ordinary]

    hlwm.call(['raise', ordinary])
    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True)[:2] \
        == [above, ordinary]

    x11.ewmh.setWmState(above_window, 2, above_atom)
    x11.sync_with_hlwm()
    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True)[:2] \
        == [ordinary, above]

    x11.ewmh.setWmState(above_window, 2, above_atom)
    x11.sync_with_hlwm()
    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True)[:2] \
        == [above, ordinary]

    x11.ewmh.setWmState(above_window, 0, above_atom)
    x11.sync_with_hlwm()
    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True)[:2] \
        == [ordinary, above]


def test_net_wm_state_above_survives_floating_changes(hlwm, x11):
    above_window, above = x11.create_client()
    _, ordinary = x11.create_client()
    x11.ewmh.setWmState(above_window, 1, '_NET_WM_STATE_ABOVE')
    x11.sync_with_hlwm()

    changes = [
        ['set_attr', f'clients.{above}.floating', 'true'],
        ['set_attr', f'clients.{above}.floating', 'false'],
        ['floating', 'on'],
        ['floating', 'off'],
    ]
    for command in changes:
        hlwm.call(command)
        hlwm.call(['raise', ordinary])
        assert helper_get_stack_as_list(hlwm, strip_focus_layer=True)[:2] \
            == [above, ordinary]


def test_fullscreen_stays_above_net_wm_state_above(hlwm, x11):
    above_window, above = x11.create_client()
    fullscreen, _ = hlwm.create_client()
    x11.ewmh.setWmState(above_window, 1, '_NET_WM_STATE_ABOVE')
    x11.sync_with_hlwm()

    hlwm.attr.clients[fullscreen].fullscreen = True
    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True)[:2] \
        == [fullscreen, above]

    hlwm.attr.clients[fullscreen].fullscreen = False
    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True)[:2] \
        == [above, fullscreen]


def test_focus_layer_stays_above_net_wm_state_above(hlwm, x11):
    above_window, above = x11.create_client()
    focused, _ = hlwm.create_client()
    x11.ewmh.setWmState(above_window, 1, '_NET_WM_STATE_ABOVE')
    x11.sync_with_hlwm()

    hlwm.attr.settings.raise_on_focus_temporarily = True
    hlwm.call(['jumpto', focused])

    assert helper_get_stack_as_list(hlwm, strip_focus_layer=False)[:2] \
        == [focused, above]


@pytest.mark.parametrize('method', ['move', 'bring'])
def test_net_wm_state_above_survives_tag_transfer(hlwm, x11, method):
    hlwm.call('add there')
    above_window, above = x11.create_client()
    x11.ewmh.setWmState(above_window, 1, '_NET_WM_STATE_ABOVE')
    x11.sync_with_hlwm()

    if method == 'move':
        hlwm.call('move there')
        hlwm.call('use there')
    else:
        hlwm.call('use there')
        hlwm.call(['bring', above])

    _, ordinary = x11.create_client()
    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True)[:2] \
        == [above, ordinary]


def test_raise_and_lower_multiple_net_wm_state_above_clients(hlwm, x11):
    hlwm.call('floating on')
    windows = [x11.create_client() for _ in range(2)]
    for window, _ in windows:
        x11.ewmh.setWmState(window, 1, '_NET_WM_STATE_ABOVE')
        x11.sync_with_hlwm()
    clients = [winid for _, winid in windows]

    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True)[:2] \
        == list(reversed(clients))
    hlwm.call(['raise', clients[0]])
    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True)[:2] \
        == clients
    hlwm.call(['lower', clients[0]])
    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True)[:2] \
        == list(reversed(clients))


def test_net_wm_state_above_request_respects_ewmhrequests(hlwm, x11):
    hlwm.call('floating on')
    above_window, above = x11.create_client()
    hlwm.attr.clients[above].ewmhrequests = False
    _, ordinary = x11.create_client()

    x11.ewmh.setWmState(above_window, 1, '_NET_WM_STATE_ABOVE')
    x11.sync_with_hlwm()

    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True)[:2] \
        == [ordinary, above]
    assert '_NET_WM_STATE_ABOVE' not in \
        x11.ewmh.getWmState(above_window, str=True)


def test_net_wm_state_above_is_preserved_with_fullscreen_state(hlwm, x11):
    window, winid = x11.create_client()
    x11.ewmh.setWmState(window, 1, '_NET_WM_STATE_ABOVE')
    x11.sync_with_hlwm()

    hlwm.attr.clients[winid].fullscreen = True
    assert set(x11.ewmh.getWmState(window, str=True)) >= {
        '_NET_WM_STATE_ABOVE',
        '_NET_WM_STATE_FULLSCREEN',
    }

    hlwm.attr.clients[winid].fullscreen = False
    assert '_NET_WM_STATE_ABOVE' in x11.ewmh.getWmState(window, str=True)


@pytest.mark.parametrize('command', ['lower', 'raise'])
def test_raise_lower_unmanaged_window(hlwm, x11, command):
    hlwm.call('rule once manage=off')
    _, winid = x11.create_client()

    assert len(hlwm.list_children('clients')) == 0, \
        "there must not be any managed client"

    # we cannot test much, but at least it does not crash
    # or throw any error:
    hlwm.call([command, winid])

    assert len(hlwm.list_children('clients')) == 0, \
        "there must not be any managed client"


def test_raise_lower_invalid_arg(hlwm):
    for command in ['raise', 'lower']:
        # both the error messages for WindowID and client should be shown:
        hlwm.call_xfail([command, 'foobar']) \
            .expect_stderr('Window id is not numeric') \
            .expect_stderr('; or:') \
            .expect_stderr("expecting 0xWINID or a description")

        hlwm.call_xfail([command, '01two']) \
            .expect_stderr('invalid characters at position 2')


def test_raise_completion(hlwm):
    assert 'urgent' in hlwm.complete('raise')

    winid, _ = hlwm.create_client()

    assert winid in hlwm.complete('raise')


def create_two_monitors_with_client_each(hlwm):
    hlwm.call('add tag2')
    hlwm.call('set_attr tags.0.floating on')
    hlwm.call('set_attr tags.1.floating on')
    hlwm.call('add_monitor 800x600+40+40 tag2')
    c1, c2 = hlwm.create_clients(2)
    hlwm.call(['load', 'tag2', '(clients max:0 {})'.format(c2)])
    return [c1, c2]


def test_new_monitor_is_on_top(hlwm):
    [c1, c2] = create_two_monitors_with_client_each(hlwm)

    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True) == [c2, c1]


def test_raise_monitor_already_on_top(hlwm):
    [c1, c2] = create_two_monitors_with_client_each(hlwm)

    hlwm.call('raise_monitor 1')

    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True) == [c2, c1]


def test_raise_monitor_2(hlwm):
    [c1, c2] = create_two_monitors_with_client_each(hlwm)

    hlwm.call('raise_monitor 0')

    assert helper_get_stack_as_list(hlwm, strip_focus_layer=True) == [c1, c2]


@pytest.mark.parametrize("here_float", [True, False])
@pytest.mark.parametrize("there_float", [True, False])
@pytest.mark.parametrize("client_float", [True, False])
@pytest.mark.parametrize("method", ['move', 'bring'])
def test_moving_with_floating(hlwm, here_float, there_float, client_float, method):
    hlwm.call('add there')
    winid, _ = hlwm.create_client()
    hlwm.call(['set_attr', 'tags.focus.floating', hlwm.bool(here_float)])
    hlwm.call(['set_attr', 'tags.by-name.there.floating', hlwm.bool(there_float)])
    hlwm.call(['set_attr', 'clients.focus.floating', hlwm.bool(client_float)])

    if method == 'move':
        hlwm.call('move there')
    else:  # method == 'bring':
        hlwm.call('use there')
        hlwm.call(['bring', winid])
        hlwm.call('use_previous')

    assert hlwm.get_attr('clients.{}.tag'.format(winid)) == 'there'
    assert winid not in helper_get_stack_as_list(hlwm)
    hlwm.call('use there')
    assert hlwm.get_attr('clients.focus.winid') == winid
    assert winid in helper_get_stack_as_list(hlwm)


def test_stack_tree(hlwm):
    # Simplified tree style:
    hlwm.call('set tree_style "     - -"')

    # Populate the stack:
    hlwm.call('add tag2')
    hlwm.call('add_monitor 800x600+40+40 tag2 monitor2')
    hlwm.create_client()
    hlwm.call('focus_monitor monitor2')
    hlwm.call('split left')
    hlwm.create_client()
    # TODO: Make one client fullscreen (doesn't seem to work yet)

    stack = hlwm.call('stack')

    expected_stack = '''\
  -
    - Monitor 1 ("monitor2") with tag "tag2"
      - Focus-Layer
      - Fullscreen-Layer
      - Above-Layer
      - Floating-Layer
      - Tiling-Layer
        - Client <windowid> "bash"
      - Frame Layer
        - Window <windowid>
        - Window <windowid>
    - Monitor 0 with tag "default"
      - Focus-Layer
      - Fullscreen-Layer
      - Above-Layer
      - Floating-Layer
      - Tiling-Layer
        - Client <windowid> "bash"
      - Frame Layer
        - Window <windowid>
    - Desktop Windows
'''
    assert strip_winids(stack.stdout) == expected_stack


def test_stack_tree_desktop_windows(hlwm, x11):

    _, winid = x11.create_client(wm_class=('TestDesktop', 'TestDeskClass'),
                                 window_type='_NET_WM_WINDOW_TYPE_DESKTOP')

    # Simplified tree style:
    hlwm.call('set tree_style "     - -"')
    framewindows = [x11.winid_str(w) for w in x11.get_hlwm_frames()]
    expected_stack = f'''\
  -
    - Monitor 0 with tag "default"
      - Focus-Layer
      - Fullscreen-Layer
      - Above-Layer
      - Floating-Layer
      - Tiling-Layer
      - Frame Layer
        - Window {framewindows[0]}
    - Desktop Windows
      - {winid} TestDesktop TestDeskClass
'''
    stack = hlwm.call('stack')
    assert stack.stdout == expected_stack


@pytest.mark.parametrize("focus_idx", range(0, 4))
def test_tag_floating_state_on(hlwm, focus_idx):
    win = hlwm.create_clients(5)
    hlwm.attr.clients[win[4]].floating = 'on'
    layout = f"""
    (split vertical:0.5:0
        (clients max:0 {win[0]} {win[1]})
        (clients max:0 {win[2]} {win[3]}))
    """.replace('\n', '')
    hlwm.call(['load', layout])
    hlwm.call(['jumpto', win[focus_idx]])

    stack_before = helper_get_stack_as_list(hlwm)
    assert stack_before[0] == win[4]  # the floating window must be on top anyway

    hlwm.call('floating on')

    # the stack should start with the floating window and the focused window
    stack_expected = [win[4], win[focus_idx]]
    # then the remaining tiled clients should follow and their order remains the same
    stack_expected += [winid for winid in stack_before if winid not in stack_expected]
    assert helper_get_stack_as_list(hlwm) == stack_expected

    # turning floating off, just restores the old stack because
    # we didn't do any raising since
    hlwm.call('floating off')
    assert helper_get_stack_as_list(hlwm) == stack_before


@pytest.mark.parametrize("focus_other_monitor", [True, False])
def test_focused_on_other_monitor_above_fullscreen(hlwm, focus_other_monitor):
    # On the unfocused monitor, put a fullscreen window
    # and focus another window. then the focused window should
    # be visible!
    #
    # Here, it does not matter if we focus this other monitor
    # and then switch back to monitor 0 (focus_other_monitor=True) or
    # whether we never have focused it (focus_other_monitor=False)
    hlwm.call('add othertag')
    hlwm.call('add_monitor 800x600+800+0')
    hlwm.call('rule tag=othertag')

    win_fullscreen, _ = hlwm.create_client()
    hlwm.attr.clients[win_fullscreen].fullscreen = 'on'

    if not focus_other_monitor:
        # if we  never want to focus the monitor with 'othertag'
        # then we need to ensure from remote that 'win_focused'
        # is the focused window
        hlwm.call('rule focus=on switchtag=off')
    win_focused, _ = hlwm.create_client()

    if focus_other_monitor:
        hlwm.call(['jumpto', win_focused])
        assert hlwm.attr.monitors.focus.index() == 1
        assert hlwm.attr.clients.focus.winid() == win_focused
        assert helper_get_stack_as_list(hlwm, strip_focus_layer=False) \
            == [win_focused, win_fullscreen]

        # go back to first monitor
        hlwm.call('focus_monitor 0')

    # even if 'othertag' is not focused, the focused window there still must be
    # above
    assert helper_get_stack_as_list(hlwm, strip_focus_layer=False) \
        == [win_focused, win_fullscreen]


@pytest.mark.parametrize("decorated", [True, False])
def test_fullscreen_above_unmanaged(hlwm, x11, mouse, decorated):
    """
    This test verifies that normal windows are below unmanaged windows (such as
    panels) and that focused fullscreen windows cover the unmanaged windows.
    """
    # create an un-managed window:
    um, um_winid = x11.create_client(geometry=(100, 100, 100, 100), force_unmanage=True)

    # create a managed window without decorations
    fs, fs_winid = x11.create_client()
    hlwm.attr.clients[fs_winid].decorated = decorated

    # move to the middle of the unmanaged window
    mouse.move_to(150, 150)
    # unmanaged windows are above normal windows:
    assert x11.get_window_under_cursor() == um

    # but below fullscreen windows:
    hlwm.attr.clients[fs_winid].fullscreen = True
    assert x11.get_window_under_cursor() == fs

    # double check that disabling fullscreen lowers the window again:
    hlwm.attr.clients[fs_winid].fullscreen = False
    assert x11.get_window_under_cursor() == um
