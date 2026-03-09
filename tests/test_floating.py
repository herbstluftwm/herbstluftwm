import pytest
from Xlib import Xutil
from herbstluftwm.types import Rectangle


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


@pytest.mark.parametrize('command', ['shift', 'resize'])
@pytest.mark.parametrize('direction', ['left', 'up'])
@pytest.mark.parametrize('put_obstacle', [True, False])
def test_directional_shift_resize_left_up(hlwm, command, direction, put_obstacle):
    """
    run: shift/resize up/left
    possibly put a window obstacle in the way
    """
    hlwm.attr.tags.focus.floating = True
    client, _ = hlwm.create_client()
    bw = 3
    hlwm.attr.theme.border_width = bw
    gap = 2
    hlwm.attr.settings.snap_gap = gap
    hlwm.attr.clients[client].sizehints_floating = False
    old_geo = Rectangle(x=160, y=170, width=80, height=90)
    hlwm.attr.clients[client].floating_geometry = old_geo

    # possibly put a window obstacle between the 'client' and the screen edge:
    if put_obstacle:
        obstacle, _ = hlwm.create_client()
        hlwm.attr.clients[obstacle].sizehints_floating = False
        obstacle_pos = 100 + bw
        if direction == 'up':
            hlwm.attr.clients[obstacle].floating_geometry = \
                Rectangle(x=0, y=0, width=400, height=100)
        elif direction == 'left':
            hlwm.attr.clients[obstacle].floating_geometry = \
                Rectangle(x=0, y=0, width=100, height=400)
    else:
        # the next obstacle is the screen edge
        obstacle_pos = 0

    if direction == 'up':
        # expected position after shift up: same x, different y
        expected_pos = (160, obstacle_pos + bw + gap)
    elif direction == 'left':
        # expected position after shift left: different x, same y
        expected_pos = (obstacle_pos + bw + gap, 170)

    hlwm.call(['jumpto', client])
    hlwm.call([command, direction])  # the actual shift/resize

    # check that the top left corner of the window is adjusted as expected
    new_geo = hlwm.attr.clients.focus.floating_geometry()
    assert expected_pos == (new_geo.x, new_geo.y)

    if command == 'shift':
        # shift means: size is unchanged
        assert (old_geo.width, old_geo.height) == (new_geo.width, new_geo.height)
    else:
        # resize means: bottom right corner is unchanged
        assert old_geo.x + old_geo.width == new_geo.x + new_geo.width
        assert old_geo.y + old_geo.height == new_geo.y + new_geo.height


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


@pytest.mark.parametrize('forward,backward', [
    ('left', 'right'),
    ('up', 'down'),
    ('right', 'left'),
    ('down', 'left'),
])
def test_focus_directional_between_identical_geometries(hlwm, forward, backward):
    hlwm.call('set_layout max')
    hlwm.call('floating on')

    # create clients with identical geometry
    clients = []
    for _ in range(0, 4):
        winid, _ = hlwm.create_client(position=(40, 50))
        clients.append(winid)

    # search the "first" client
    status = 0
    while status == 0:
        status = hlwm.unchecked_call(['focus', backward]).returncode

    # go in the opposite direction and check that we traverse all clients
    traversed = []
    status = 0
    while status == 0:
        traversed.append(hlwm.attr.clients.focus.winid())
        status = hlwm.unchecked_call(['focus', forward]).returncode

    assert sorted(traversed) == sorted(clients)


@pytest.mark.parametrize('direction', ['up', 'down', 'left', 'right'])
def test_resize_shrink_client(hlwm, direction):
    hlwm.attr.settings.snap_gap = 5
    hlwm.attr.tags.focus.floating = True
    hlwm.attr.monitors.focus.geometry = Rectangle(x=0, y=0, width=800, height=900)

    hlwm.create_client()
    hlwm.attr.clients.focus.sizehints_floating = False
    # place the client roughly in the center of the monitor:
    geo_orig = Rectangle(x=200, y=200, width=300, height=400)
    hlwm.attr.clients.focus.floating_geometry = geo_orig
    # shift the client towards the monitor edge in the specified 'direction'
    hlwm.call(['shift', direction])
    geo_moved = hlwm.attr.clients.focus.floating_geometry()
    # and the size is as originally
    assert geo_moved.width == geo_orig.width
    assert geo_moved.height == geo_orig.height

    # calling 'resize' with the same direction forces the client to shrink:
    hlwm.call(['resize', direction])

    geo_shrunk = hlwm.attr.clients.focus.floating_geometry()
    if direction in ['up', 'down']:
        # client was shrunk in height
        assert geo_shrunk.width == geo_moved.width
        assert geo_shrunk.height == geo_moved.height // 2
    else:
        # client was shrunk in width
        assert geo_shrunk.width == geo_moved.width // 2
        assert geo_shrunk.height == geo_moved.height

    if direction in ['right', 'down']:
        # bottom right corner stays unchanged
        assert geo_shrunk.x + geo_shrunk.width == geo_moved.x + geo_moved.width
        assert geo_shrunk.y + geo_shrunk.height == geo_moved.y + geo_moved.height
    else:
        # top left corner stays unchanged
        assert geo_shrunk.x == geo_moved.x
        assert geo_shrunk.y == geo_moved.y


@pytest.mark.parametrize('direction', ['up', 'down', 'left', 'right'])
def test_resize_shrink_not_possible(hlwm, direction):
    hlwm.attr.settings.snap_gap = 5
    hlwm.attr.tags.focus.floating = True

    winid, _ = hlwm.create_client()
    hlwm.attr.clients.focus.sizehints_floating = False
    # place the client roughly in the center of the monitor:
    geo_orig = Rectangle(x=200, y=200, width=160, height=140)
    hlwm.attr.clients.focus.floating_geometry = geo_orig

    # call resize multiple times
    for i in range(0, 15):
        proc = hlwm.unchecked_call(['resize', direction])
        if proc.returncode != 0:
            assert f"{winid} is too small to be shrunk" in proc.stderr
            break
    geo_final = hlwm.attr.clients.focus.floating_geometry()

    # after calling 'resize' that often, the minimum window size
    # must have been reached. So calling it again will not change the geometry:
    hlwm.call_xfail(['resize', direction]).expect_stderr('too small')
    assert geo_final == hlwm.attr.clients.focus.floating_geometry()

    if direction in ['up', 'down']:
        assert geo_orig.width == geo_final.width
    else:
        assert geo_orig.height == geo_final.height

    assert geo_final.width >= 50
    assert geo_final.height >= 50


@pytest.mark.parametrize('floating', [True, False])
def test_sizehint_change_applied(hlwm, x11, floating):
    hlwm.attr.tags.focus.floating = floating
    # create a client with a geometry that does not match the sizehints
    # we will set later
    handle, winid = x11.create_client()
    old_geometry = Rectangle(x=30, y=40, width=111, height=121)
    hlwm.attr.clients[winid].floating_geometry = old_geometry
    hlwm.attr.clients[winid].sizehints_floating = True
    if floating:
        assert hlwm.attr.clients[winid].content_geometry().size() == old_geometry.size()

    # update the size hints:
    hints = {
        'flags': Xutil.PResizeInc,
        'width_inc': 5,
        'height_inc': 7,
    }
    handle.set_wm_normal_hints(hints)
    x11.sync_with_hlwm()

    new_geometry = hlwm.attr.clients[winid].floating_geometry()
    # the new size adheres to the size hints
    assert new_geometry.width % hints['width_inc'] == 0
    assert new_geometry.height % hints['height_inc'] == 0
    # the new size changes only as little as necessary
    assert abs(new_geometry.width - old_geometry.width) <= hints['width_inc']
    assert abs(new_geometry.height - old_geometry.height) <= hints['height_inc']

    if floating:
        assert hlwm.attr.clients[winid].content_geometry().size() == new_geometry.size()
