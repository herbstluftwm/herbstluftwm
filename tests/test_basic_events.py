import pytest
import test_stack
import Xlib


@pytest.mark.parametrize("single_floating", [True, False])
@pytest.mark.parametrize("raise_on_click", [True, False])
def test_focus_on_click(hlwm, mouse, raise_on_click, single_floating):
    if single_floating:
        hlwm.call('rule floating=on')
    else:
        hlwm.call('set_attr tags.focus.floating on')
    hlwm.call(['set', 'raise_on_click', hlwm.bool(raise_on_click)])
    c1, _ = hlwm.create_client(position=(0, 0))
    c2, _ = hlwm.create_client(position=(300, 0))
    hlwm.call(f'jumpto {c2}')  # also raises c2
    assert hlwm.get_attr('clients.focus.winid') == c2
    assert test_stack.helper_get_stack_as_list(hlwm) == [c2, c1]

    mouse.click('1', into_win_id=c1)

    assert hlwm.get_attr('clients.focus.winid') == c1
    stack = test_stack.helper_get_stack_as_list(hlwm)
    if raise_on_click:
        # c1 gets back on top
        assert stack == [c1, c2]
    else:
        # c2 stays on top
        assert stack == [c2, c1]


@pytest.mark.parametrize("single_floating", [True, False])
@pytest.mark.parametrize("focus_follows_mouse", [True, False])
def test_focus_follows_mouse(hlwm, mouse, focus_follows_mouse, single_floating):
    if single_floating:
        hlwm.call('rule floating=on')
    else:
        hlwm.call('set_attr tags.focus.floating on')
    hlwm.call('set_attr tags.focus.floating on')
    hlwm.call(['set', 'focus_follows_mouse', hlwm.bool(focus_follows_mouse)])
    mouse.move_to(0, 0)  # make sure that the cursor is not within any of the new windows
    c1, _ = hlwm.create_client(position=(10, 0))
    c2, _ = hlwm.create_client(position=(300, 0))
    hlwm.call(f'jumpto {c2}')  # also raises c2
    assert hlwm.get_attr('clients.focus.winid') == c2
    assert test_stack.helper_get_stack_as_list(hlwm) == [c2, c1]

    mouse.move_into(c1)

    c1_is_focused = hlwm.get_attr('clients.focus.winid') == c1
    # c1 is focused iff focus_follows_mouse was activated
    assert c1_is_focused == focus_follows_mouse
    # stacking is unchanged
    assert test_stack.helper_get_stack_as_list(hlwm) == [c2, c1]


@pytest.mark.parametrize("click", [True, False])
@pytest.mark.parametrize("focus_follows_mouse", [True, False])
def test_focus_frame_by_mouse(hlwm, mouse, click, focus_follows_mouse):
    hlwm.call('set always_show_frame on')
    hlwm.call('set frame_bg_transparent off')
    hlwm.call(['set', 'focus_follows_mouse', hlwm.bool(focus_follows_mouse)])
    layout_template = '(split horizontal:0.5:{}'
    layout_template += ' (clients vertical:0)'
    layout_template += ' (clients vertical:0))'
    # a layout where the left frame is focused
    hlwm.call(['load', layout_template.format(0)])
    assert layout_template.format(0) == hlwm.call('dump').stdout
    width, height = [int(v) for v in hlwm.call('monitor_rect').stdout.split(' ')][2:]

    # go to left frame
    mouse.move_to(width * 0.25, height * 0.5)

    # go to right frame
    mouse.move_to(width * 0.75, height * 0.5)
    if click:
        mouse.click('1')

    # we expect the focus to go to the second frame
    # if focus_follows_mouse is enabled or if we click explicitly
    expected_focus = 1 if focus_follows_mouse or click else 0
    assert layout_template.format(expected_focus) \
        == hlwm.call('dump').stdout


@pytest.mark.parametrize("click", [True, False])
@pytest.mark.parametrize("focus_follows_mouse", [True, False])
def test_focus_client_by_decoration(hlwm, mouse, x11, click, focus_follows_mouse):
    hlwm.attr.theme.border_width = 1
    for side in ['top', 'bottom', 'left', 'right']:
        hlwm.attr.theme[f'padding_{side}'] = 49
    hlwm.attr.theme.active.color = 'red'
    hlwm.attr.theme.normal.color = 'blue'
    hlwm.call('set_layout vertical')
    hlwm.call(['set', 'focus_follows_mouse', hlwm.bool(focus_follows_mouse)])
    winids = hlwm.create_clients(2)
    hlwm.call(['jumpto', winids[0]])
    assert hlwm.get_attr('clients.focus.winid') == winids[0]

    # move mouse to the decoration:
    decwin = x11.get_decoration_window(x11.window(winids[1]))
    # here, xdotool hangs on the second invokation of this testcase
    mouse.move_into(x11.winid_str(decwin), 10, 10)
    if click:
        mouse.click('1')
    expected_focus = 1 if focus_follows_mouse or click else 0
    assert hlwm.get_attr('clients.focus.winid') == winids[expected_focus]


def test_drop_enter_notify_events(hlwm, mouse):
    """test that tiling/resizing the clients does not trigger
    mouse enter notifications
    """
    hlwm.call('set focus_follows_mouse on')
    # place two clients in frames side by side
    winid = hlwm.create_clients(2)
    layout = '(split horizontal:0.5:0 (clients max:0 {}) (clients max:0 {}))'
    layout = layout.format(winid[0], winid[1])
    hlwm.call(['load', layout])

    # place the mouse on the right window, but place it
    # close to the edge, round by the corner
    mouse.move_into(winid[1], 10, 10)
    assert hlwm.get_attr('clients.focus.winid') == winid[1]

    # Sudden 'resize' call shouldn't take away the focus
    hlwm.call('resize right 0.4')
    assert hlwm.get_attr('clients.focus.winid') == winid[1]


@pytest.mark.parametrize("client_count", [7, 8, 9, 10])
def test_enternotify_do_not_drop_events(hlwm, mouse, client_count):
    """test that when triggering multiple enter notify events
    that the last enter notify event survives
    """
    hlwm.call('set focus_follows_mouse on')
    winid = hlwm.create_clients(client_count)

    # place many enter notify events in the event queue
    for i in range(0, client_count):
        # here, it's important that move_into does not sync with hlwm
        # such that the event queue in hlwm builds up
        mouse.move_into(winid[i], 10, 10, wait=False)

    # finally, all enter notify events must survive
    assert hlwm.get_attr('clients.focus.winid') == winid[client_count - 1]


def test_no_map_of_visible_unmanaged_windows(hlwm, hc_idle, x11):
    hlwm.call(['rule', 'hook=tempmanage'])
    x11.pop_pending_events()  # clear event queue

    def pre_map(winhandle):
        # get MapNotify events
        winhandle.change_attributes(event_mask=Xlib.X.StructureNotifyMask)
        x11.display.grab_server()

    def pre_sync(winhandle):
        # right after mapping, unmap the window again.
        # Hence, we will be noticed if hlwm maps the window without permission
        winhandle.unmap()
        x11.display.ungrab_server()

    handle, winid = x11.create_client(force_unmanage=True, pre_map=pre_map, pre_sync=pre_sync)
    map_notify_events = []
    events = x11.pop_pending_events()
    for e in events:
        if e.type == Xlib.X.MapNotify:
            map_notify_events.append(e)

    # the rules are evaluated correctly:
    assert ['rule', 'tempmanage', winid] in hc_idle.hooks()
    # and hlwm didn't try to call XMapWindow:
    assert len(map_notify_events) == 1
