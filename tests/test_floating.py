import pytest


@pytest.mark.parametrize('clientFocused', [0, 1, 2, 3])
def test_directional_focus(hlwm, clientFocused):
    hlwm.call('attr tags.focus.floating on')
    monrect = hlwm.call('monitor_rect').stdout.split(' ')
    width = int(monrect[2])
    height = int(monrect[3])
    client_pos = [
        (0, 0),                     # position of 0
        (width / 2, 0),             # position of 1
        (0, height / 2),            # position of 2
        (width / 2, height / 2),    # position of 3
    ]
    clients = []
    for pos in client_pos:
        winid, _ = hlwm.create_client(position=pos)
        clients.append(winid)
    # clients:
    #    +---+---+
    #    | 0 | 1 |
    #    +---+---+
    #    | 2 | 3 |
    #    +---+---+
    focusDirs = ['up', 'down', 'left', 'right']
    client2neighbours = [
        [None, 2, None, 1],  # neighbours of 0
        [None, 3, 0, None],  # neighbours of 1
        [0, None, None, 3],  # neighbours of 2
        [1, None, 2, None],  # neighbours of 3
    ]

    for idx, d in enumerate(focusDirs):
        # go to 'clientFocused'
        hlwm.call(['jumpto', clients[clientFocused]])

        cmd = ['focus', d]
        expected_target = client2neighbours[clientFocused][idx]
        if expected_target is not None:
            hlwm.call(cmd)
            assert hlwm.get_attr('clients.focus.winid') \
                == clients[expected_target]
        else:
            hlwm.call_xfail(cmd).expect_stderr('No neighbour found')


@pytest.mark.parametrize('start_y_rel,hits_obstacle', [
    (0, False),  # passes the other window above
    (0.2, False),  # passes it above
    (0.45, True),
    (0.55, True),
    (0.78, False),  # passes it below (hope that snap_gap isn't too big)
])
def test_directional_shift_right(hlwm, x11, start_y_rel, hits_obstacle):
    hlwm.call('attr tags.focus.floating on')
    hlwm.call('attr theme.border_width 0')
    snap_gap = 8
    hlwm.call(f'attr settings.snap_gap {snap_gap}')
    monrect = hlwm.call('monitor_rect').stdout.split(' ')
    width = int(monrect[2])
    height = int(monrect[3])

    start_y = int(height * start_y_rel)
    winwidth = int(width / 5)
    winheight = int(height / 5)
    handle, winid = x11.create_client(geometry=(0, start_y, winwidth, winheight))
    # create the obstacle
    obs_x = width // 2
    obs_y = height // 2
    x11.create_client(geometry=(obs_x, obs_y, width // 4, height // 4))
    hlwm.call(['jumpto', winid])

    if hits_obstacle:
        hlwm.call('shift right')
        x, y = x11.get_absolute_top_left(handle)
        assert y == start_y
        assert x + winwidth + snap_gap == obs_x

    # hits monitor edge
    hlwm.call('shift right')
    x, y = x11.get_absolute_top_left(handle)
    assert y == start_y
    assert x + winwidth + snap_gap == width


def test_floating_command_no_tag(hlwm):
    assert hlwm.get_attr('tags.0.floating') == hlwm.bool(False)

    # toggles the floating state of current tag
    hlwm.call('floating')
    assert hlwm.get_attr('tags.0.floating') == hlwm.bool(True)

    # toggles the floating state again
    hlwm.call('floating')
    assert hlwm.get_attr('tags.0.floating') == hlwm.bool(False)


def test_bring_floating_from_different_tag(hlwm, x11):
    win, winid = x11.create_client()
    hlwm.call('true')
    hlwm.call(f'set_attr clients.{winid}.floating true')
    hlwm.call('add anothertag')
    hlwm.call('use anothertag')
    assert hlwm.get_attr(f'clients.{winid}.tag') == 'default'
    hlwm.call(['bring', winid])

    assert hlwm.get_attr(f'clients.{winid}.tag') == 'anothertag'
    assert hlwm.get_attr(f'clients.{winid}.floating') == hlwm.bool(True)


@pytest.mark.parametrize('othertag', [True, False])
@pytest.mark.parametrize('floating', [True, False])
def test_bring_minimized_window(hlwm, othertag, floating):
    if othertag:
        hlwm.call('add othertag')
        hlwm.call('rule tag=othertag')
    hlwm.call('chain , split explode , split explode , split explode')
    winid, _ = hlwm.create_client()
    hlwm.call(f'set_attr clients.{winid}.minimized on')
    hlwm.call(f'set_attr clients.{winid}.floating {hlwm.bool(floating)}')
    # also focus another frame than one that carried the client
    hlwm.call('cycle_frame')
    # and remember which frame was focused before 'bring' is called
    frame_focused = hlwm.get_attr('tags.focus.tiling.focused_frame.index')

    hlwm.call(f'bring {winid}')

    assert hlwm.get_attr('clients.focus.winid') == winid
    assert hlwm.get_attr('clients.focus.minimized') == hlwm.bool(False)
    assert hlwm.get_attr('clients.focus.visible') == hlwm.bool(True)
    assert hlwm.get_attr('clients.focus.floating') == hlwm.bool(floating)
    assert frame_focused == hlwm.get_attr('tags.focus.tiling.focused_frame.index'), \
        "'bring' must not change the frame focus"


@pytest.mark.parametrize('direction', ['down', 'right', ])
def test_resize_floating_client(hlwm, x11, direction):
    hlwm.call('attr settings.snap_gap 0')
    client, winid = x11.create_client()
    mon_width, mon_height = [int(v) for v in
                             hlwm.call('monitor_rect').stdout.split(' ')[2:]]
    hlwm.call(f'set_attr clients.{winid}.floating true')
    x_before, y_before = x11.get_absolute_top_left(client)
    geom_before = client.get_geometry()
    assert (geom_before.width, geom_before.height) == (300, 200)

    hlwm.call(['resize', direction])

    # the position has not changed
    assert (x_before, y_before) == x11.get_absolute_top_left(client)
    # but the size grew up to the monitor edge
    geom_after = client.get_geometry()
    if direction == 'right':
        assert x_before + geom_after.width == mon_width
        assert geom_after.height == geom_before.height
    if direction == 'down':
        assert y_before + geom_after.height == mon_height
        assert geom_after.width == geom_before.width
