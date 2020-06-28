import pytest
import re
import math
import textwrap
from decimal import Decimal


@pytest.mark.parametrize("running_clients_num", [0, 1, 2])
def test_single_frame_layout(hlwm, running_clients, running_clients_num):
    assert hlwm.get_attr('tags.0.curframe_windex') == '0'
    assert int(hlwm.get_attr('tags.0.client_count')) == running_clients_num
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == running_clients_num


def test_single_frame_layout_three(hlwm):
    hlwm.create_clients(3)
    assert hlwm.get_attr('tags.0.curframe_windex') == '0'
    assert int(hlwm.get_attr('tags.0.client_count')) == 3
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == 3


@pytest.mark.parametrize("running_clients_num", [2, 3])
def test_explode(hlwm, running_clients, running_clients_num):
    assert running_clients_num >= 2, "explode behaves as auto for one client"
    hlwm.call('split explode')
    assert hlwm.get_attr('tags.0.curframe_windex') == '0'
    number_upper = (running_clients_num + 1) // 2
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == number_upper
    assert int(hlwm.get_attr('tags.0.client_count')) == running_clients_num
    assert hlwm.get_attr('tags.0.frame_count') == '2'


@pytest.mark.parametrize("running_clients_num", [0, 1, 4])
def test_remove(hlwm, running_clients, running_clients_num):
    hlwm.call('split explode')
    hlwm.call('remove')
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == running_clients_num
    assert int(hlwm.get_attr('tags.0.client_count')) == running_clients_num
    assert hlwm.get_attr('tags.0.frame_count') == '1'


@pytest.mark.parametrize("running_clients_num", [4])
@pytest.mark.parametrize("focus_idx", [0, 1, 2, 3])
def test_remove_client_focus(hlwm, running_clients, running_clients_num, focus_idx):
    layout = """
        (split horizontal:0.5:{x}
          (clients vertical:{y} {} {})
          (clients vertical:{z} {} {}))
    """.format(*running_clients,
               x=(focus_idx // 2),
               y=(focus_idx % 2),
               z=(focus_idx % 2)).strip()
    hlwm.call(['load', layout])
    focus = hlwm.get_attr('clients.focus.winid')
    assert running_clients[focus_idx] == focus

    hlwm.call('remove')

    # then the focus is preserved
    assert focus == hlwm.get_attr('clients.focus.winid')
    # and the clients appear in the original order
    assert ' '.join(running_clients) in hlwm.call('dump').stdout
    assert hlwm.get_attr('tags.0.frame_count') == '1'


@pytest.mark.parametrize("running_clients_num", [3])
def test_remove_nested_split(hlwm, running_clients, running_clients_num):
    # a layout with three frames side by side
    # where the right frame is focused.
    # in the frame containing the left and the middle frame,
    # the selection is 0, so new clients there would be inserted
    # into the left frame. However, when removing the right frame, its
    # clients should go to the (formerly) middle frame
    layout = """
        (split horizontal:0.5:1
          (split horizontal:0.5:0
              (clients vertical:0 {})
              (clients vertical:0 {}))
          (clients vertical:0 {}))
    """.format(*running_clients).strip()
    hlwm.call(['load', layout])
    assert running_clients[2] == hlwm.get_attr('clients.focus.winid')

    hlwm.call('remove')  # remove the right-most frame

    # the clients of the formerly right frame go to the formerly
    # middle frame
    assert running_clients == re.findall(r'0x[0-9a-f]+', hlwm.call('dump').stdout)
    assert hlwm.get_attr('tags.0.frame_count') == '2'


@pytest.mark.parametrize("running_clients_num", [3, 4])
def test_focus_wrap(hlwm, running_clients, running_clients_num):
    assert running_clients_num >= 2, "explode behaves as auto for one client"
    # TODO: create the layout explicitly once we have the 'load' command again
    #       instead of calling explode
    hlwm.call('split explode')
    for i in range(0, running_clients_num):
        expected_idx = i % ((running_clients_num + 1) // 2)
        assert int(hlwm.get_attr('tags.0.curframe_windex')) == expected_idx
        if i < running_clients_num - 1:
            hlwm.call('focus down')


@pytest.mark.parametrize("path", '@ 0 1 00 11 . / /. ./'.split(' '))
@pytest.mark.parametrize("running_clients_num", [3])
@pytest.mark.parametrize("num_splits", [0, 1, 2])
def test_dump(hlwm, running_clients, path, running_clients_num, num_splits):
    for i in range(0, num_splits):
        hlwm.call('split explode')

    layout = hlwm.call('dump').stdout
    layout_part = hlwm.call('dump "" ' + path).stdout

    if num_splits > 0:
        assert layout_part in layout
        assert len(layout_part) < len(layout)
    else:
        assert layout_part == layout


def test_dump_invalid_tag_name(hlwm):
    hlwm.call_xfail('dump foo') \
        .expect_stderr('dump: Tag "foo" not found')


def test_dump_frame_index(hlwm):
    layout = {}
    layout['00'] = "(clients vertical:0)"
    layout['01'] = "(clients grid:0)"
    layout['0'] = '(split vertical:0.7:0 {} {})'.format(
        layout['00'], layout['01'])
    layout['1'] = '(clients horizontal:0)'
    layout[''] = '(split horizontal:0.65:0 {} {})'.format(
        layout['0'], layout['1'])
    hlwm.call(['load', layout['']])

    tag = hlwm.get_attr('tags.focus.name')
    for index in ["", "0", "1", "00", "01"]:
        assert hlwm.call(['dump', tag, index]).stdout == layout[index]


@pytest.mark.parametrize("command", ['dump', 'layout'])
def test_dump_layout_other_tag(hlwm, command):
    hlwm.call('add other')
    hlwm.call('split explode')

    assert hlwm.call([command]).stdout \
        != hlwm.call([command, 'other']).stdout


@pytest.mark.parametrize("running_clients_num", [0, 5])
@pytest.mark.parametrize("num_splits", [0, 2])
@pytest.mark.parametrize("cycle_delta", [-2, -1, 0, 1, 2])
def test_cycle(hlwm, running_clients, running_clients_num, num_splits, cycle_delta):
    for i in range(0, num_splits):
        hlwm.call('split explode')
    windex = int(hlwm.get_attr('tags.0.curframe_windex'))
    wcount = int(hlwm.get_attr('tags.0.curframe_wcount'))
    hlwm.call('cycle {}'.format(cycle_delta))
    new_windex = int(hlwm.get_attr('tags.0.curframe_windex'))
    expected_index = (windex + cycle_delta + wcount) % wcount if wcount > 0 else 0
    assert expected_index == new_windex


@pytest.mark.parametrize("running_clients_num", [0, 1, 5])
@pytest.mark.parametrize("index", [0, 1, 3, 5])
def test_focus_nth(hlwm, running_clients, running_clients_num, index):
    hlwm.call('focus_nth {}'.format(index))
    windex = int(hlwm.get_attr('tags.0.curframe_windex'))
    assert windex == max(0, min(index, running_clients_num - 1))


@pytest.mark.parametrize("running_clients_num", [5])
def test_rotate(hlwm, running_clients, running_clients_num):
    # generate some layout with clients in it
    for i in range(0, 3):
        hlwm.call('split explode')
    # rotate 4 times and remember the layout before
    layouts = []
    for i in range(0, 4):
        layouts.append(hlwm.call('dump').stdout)
        hlwm.call('rotate')
    # then the final layout matches the first
    assert hlwm.call('dump').stdout == layouts[0]
    # but all the intermediate layouts are distinct
    for i1, l1 in enumerate(layouts):
        for i2, l2 in enumerate(layouts[0:i1]):
            assert l1 != l2


@pytest.mark.parametrize("running_clients_num", [5])
@pytest.mark.parametrize("num_splits", [0, 1, 2, 3])
def test_layout_command(hlwm, running_clients, running_clients_num, num_splits):
    hlwm.call('set tree_style "     C S"')
    # create some layout
    for i in range(0, num_splits):
        hlwm.call('split explode')

    layout_str = hlwm.call('layout').stdout

    # we're on the focused tag, so:
    assert '[FOCUS]' in layout_str
    layout_str = layout_str.replace(' [FOCUS]', '')

    lines = [line.strip() for line in layout_str.splitlines()]
    # count the client frames
    assert len([line for line in lines if line[0] == 'C']) \
        == int(hlwm.get_attr('tags.0.frame_count'))
    # count the split frames
    assert len([line for line in lines if line[0] == 'S']) == num_splits
    # transform dump-output to something similar
    dumped = hlwm.call('dump').stdout \
        .replace('(', '\n').replace(')', '').strip() \
        .replace('split', 'S').replace('clients', 'C')
    # remove all information except for align/layout names
    dumped = re.sub(r':[^ ]*', '', dumped)
    dumped = re.sub(r'[ ]*\n', '\n', dumped)
    lines = [re.sub(r' [0-9]+% selection=[01]', '', line) for line in lines]
    assert dumped == '\n'.join(lines).replace(':', '')


@pytest.mark.parametrize("running_clients_num,client2focus",
                         [(cnt, idx) for cnt in [3, 5] for idx in range(0, cnt)])
@pytest.mark.parametrize("num_splits", [0, 1, 2, 3])
def test_jumpto_within_tag(hlwm, running_clients, client2focus, num_splits):
    for i in range(0, num_splits):
        hlwm.call('split explode')
    c = running_clients[client2focus]
    if client2focus != 0:
        assert hlwm.get_attr('clients.focus.winid') != c

    hlwm.call(['jumpto', c])

    assert hlwm.get_attr('clients.focus.winid') == c


@pytest.mark.parametrize("running_clients_num", [2, 5])
@pytest.mark.parametrize("num_splits", [0, 1, 2, 3])
@pytest.mark.parametrize("delta", [1, -1])
def test_cycle_all_traverses_all(hlwm, running_clients, num_splits, delta):
    for i in range(0, num_splits):
        hlwm.call('split explode')

    visited_winids = []
    for _ in range(0, len(running_clients) + num_splits):
        # if a client is focused, then read its window-id
        w = hlwm.call('try and , silent get_attr clients.focus.winid \
                               , get_attr clients.focus.winid').stdout
        hlwm.call(['cycle_all', delta])  # go the next window
        if w == '':
            continue  # ignore empty frames
        if w in visited_winids:
            break  # stop if we were at window seen before
        visited_winids.append(w)

    # winids should hold all windows in the correct order
    all_winids = [m.group(0) for m in re.finditer(r'0x[^ )]*', hlwm.call('dump').stdout)]
    if delta == -1:
        all_winids = [all_winids[0]] + list(reversed(all_winids[1:]))
    assert all_winids == visited_winids


@pytest.mark.parametrize("running_clients_num", [4])
@pytest.mark.parametrize("delta", [1, -1])
def test_cycle_all_skip_invisible(hlwm, running_clients, delta):
    # TODO: create better test case when we have the 'load' command again
    # create two frames both in max mode, with 3 and 2 clients
    hlwm.call('set_layout max')
    hlwm.call('split explode')
    layout = hlwm.call('dump').stdout

    visited_winids = []
    for i in range(0, 2):
        visited_winids.append(hlwm.get_attr('clients.focus.winid'))
        hlwm.call(['cycle_all', '--skip-invisible', delta])

    # we are in the same situation as before
    assert layout == hlwm.call('dump').stdout
    assert visited_winids[0] == hlwm.get_attr('clients.focus.winid')
    # but we visited two different windows
    assert visited_winids[0] != visited_winids[1]


@pytest.mark.parametrize("num_tiling", [1, 2])
@pytest.mark.parametrize("num_floating", [0, 2])
@pytest.mark.parametrize("delta", [1, -1])
def test_cycle_all_traverses_tiling_and_floating(hlwm, delta, num_tiling, num_floating):
    tiled = hlwm.create_clients(num_tiling)
    floated = hlwm.create_clients(num_floating)
    for c in floated:
        hlwm.call(f'set_attr clients.{c}.floating true')
    expected_clients = tiled + floated + [tiled[0]]
    if delta == -1:
        expected_clients = reversed(expected_clients)

    for focus in expected_clients:
        assert focus == hlwm.get_attr('clients.focus.winid')
        hlwm.call(['cycle_all', delta])


@pytest.mark.parametrize("delta", [1, -1])
def test_cycle_all_empty_frame(hlwm, delta):
    layout = hlwm.call('dump').stdout

    hlwm.call(['cycle_all', delta])

    assert layout == hlwm.call('dump').stdout


@pytest.mark.parametrize("floating_clients", [1, 2])
@pytest.mark.parametrize("delta", [1, -1])
def test_cycle_all_with_floating_clients(hlwm, delta, floating_clients):
    hlwm.call('rule floating=on')
    clients = hlwm.create_clients(floating_clients)
    # None = No client is selected in the tiling layer
    traversal = [None] + clients + [None]
    if delta == -1:
        traversal = reversed(traversal)

    for expected_focus in traversal:
        if expected_focus is None:
            assert 'focus' not in hlwm.list_children('clients')
        else:
            assert expected_focus == hlwm.get_attr('clients.focus.winid')
        hlwm.call(['cycle_all', delta])


@pytest.mark.parametrize("running_clients_num", [2, 5])
@pytest.mark.parametrize("num_splits", [0, 1, 2, 3])
@pytest.mark.parametrize("delta", [1, -1])
def test_cycle_frame_traverses_all(hlwm, running_clients, num_splits, delta):
    for i in range(0, num_splits):
        hlwm.call('split explode')

    # cycle through all of our (num_splits+1)-many frames
    layouts = []
    for i in range(0, num_splits + 1):
        layouts.append(hlwm.call('dump').stdout)
        hlwm.call(['cycle_frame', delta])

    # then we're back on the original layout
    assert layouts[0] == hlwm.call('dump').stdout
    # and all intermediate layouts are different
    for i1 in range(0, len(layouts)):
        for i2 in range(0, i1):
            assert layouts[i1] != layouts[i2]


def test_cycle_frame_invalid_delta(hlwm):
    hlwm.call_xfail(['cycle_frame', 'df8']) \
        .expect_stderr('invalid argument: stoi')
    hlwm.call_xfail(['cycle_frame', '-230984209340']) \
        .expect_stderr('out of range')


@pytest.mark.parametrize("running_clients_num", [0, 2])
@pytest.mark.parametrize("align", ["horizontal", "vertical"])
@pytest.mark.parametrize("fraction", ["0.1", "0.5", "0.7"])
def test_split_simple(hlwm, running_clients, align, fraction):
    hlwm.call(['split', align, fraction])
    assert hlwm.call('dump').stdout == \
        '(split {}:{}:0 (clients vertical:0{}) (clients vertical:0))' \
        .format(align, fraction, ''.join([' ' + c for c in running_clients]))


def test_split_invalid_alignment(hlwm):
    hlwm.call_xfail('split foo') \
        .expect_stderr('split: Invalid alignment "foo"')


@pytest.mark.parametrize("align", ["horizontal", "vertical"])
def test_split_and_remove_with_smart_frame_surroundings(hlwm, x11, align):
    # Split frame, then merge it again to one root frame
    # Root frame should have no frame gaps in the end
    hlwm.call('set smart_frame_surroundings on')
    hlwm.call(['split', align])
    hlwm.call('remove')

    # Search for all frames, there should only be one
    frame_x11 = x11.get_hlwm_frames()[0]
    frame_geom = frame_x11.get_geometry()
    assert (frame_geom.width, frame_geom.height) == (800, 600)


@pytest.mark.parametrize("client_focused", list(range(0, 4)))
@pytest.mark.parametrize("direction", ['u', 'd', 'l', 'r'])
def test_focus_directional_2x2grid(hlwm, client_focused, direction):
    clients = hlwm.create_clients(4)
    layout = '(split horizontal:0.5:0 (clients vertical:0 W W) (clients vertical:1 W W))'
    client2direction = [
        {'u': None, 'd': 1, 'l': None, 'r': 3},
        {'u': 0, 'd': None, 'l': None, 'r': 3},
        {'u': None, 'd': 3, 'l': 0, 'r': None},
        {'u': 2, 'd': None, 'l': 0, 'r': None},
    ]
    for winid in clients:
        # replace the next W by the window ID
        layout = layout.replace('W', winid, 1)
    hlwm.call(['load', layout])
    hlwm.call(['jumpto', clients[client_focused]])
    # get expected neighbour:
    neighbour = client2direction[client_focused][direction]

    if neighbour is not None:
        hlwm.call(['focus', direction])
        assert hlwm.get_attr('clients.focus.winid') == clients[neighbour]
    else:
        hlwm.call_xfail(['focus', direction]) \
            .expect_stderr('No neighbour')


@pytest.mark.parametrize("border_width", [0, 2])
@pytest.mark.parametrize("minimal_border_width", [0, 7])
def test_smart_window_surroundings(hlwm, x11, border_width, minimal_border_width):
    hlwm.call('set_layout vertical')
    hlwm.call('set frame_gap 0')
    mbw = minimal_border_width
    hlwm.call(f'attr theme.minimal.border_width {mbw}')
    hlwm.call(f'set window_border_width {border_width}')
    hlwm.call('set frame_border_width 0')
    hlwm.call('set frame_padding 0')
    hlwm.call('set smart_window_surroundings on')
    window_gap = 10
    hlwm.call(f'set window_gap {window_gap}')
    mon_rect_str = hlwm.call('monitor_rect').stdout
    mon_width, mon_height = [int(v) for v in mon_rect_str.split(' ')[2:4]]
    # works only if mon_height is even...

    # with only one client and smart_window_surroundings, no window gap is
    # applied and the minimal decoration scheme is used
    win1, _ = x11.create_client()
    geo1 = win1.get_geometry()
    assert (geo1.x, geo1.y) == (mbw, mbw)
    assert (geo1.width + 2 * mbw, geo1.height + 2 * mbw) \
        == (mon_width, mon_height)

    # but window_gap with the second one
    win2, _ = x11.create_client()
    geo1 = win1.get_geometry()
    geo2 = win2.get_geometry()
    # in vertical layout they have the same size
    assert (geo1.width, geo1.height) == (geo2.width, geo2.height)
    # we have twice the window gap (left and right) and twice the border_width
    assert geo1.width == mon_width - 2 * window_gap - 2 * border_width
    # we have three times the window gap in height (below, above, and between
    # the client windows) and four times the border width (below and above
    # every client)
    assert geo1.height + geo2.height + 3 * window_gap + 4 * border_width \
        == mon_height


@pytest.mark.parametrize('running_clients_num,start_idx_range', [
    # number of clients and indices where we should start
    (6, range(0, 6)),
    (7, [3, 4, 5, 6]),  # only check last two rows
    (8, [3, 4, 5, 6]),  # only check last two rows
    (9, [6, 7, 8]),   # only last row
])
@pytest.mark.parametrize('gapless_grid', [True, False])
def test_grid_neighbours_3_columns(hlwm, running_clients, running_clients_num,
                                   start_idx_range, gapless_grid):
    hlwm.call(['set', 'gapless_grid', hlwm.bool(gapless_grid)])
    direction2coordinates = {
        # row, column
        'up': (-1, 0),
        'down': (1, 0),
        'left': (0, -1),
        'right': (0, 1),
    }
    for start_idx in start_idx_range:
        layout = '(clients grid:{} {})'
        layout = layout.format(start_idx, ' '.join(running_clients))

        column_count = 3
        row_count = math.ceil(running_clients_num / column_count)
        column = start_idx % 3
        row = start_idx // 3

        for direction, (dy, dx) in direction2coordinates.items():
            hlwm.call(['load', layout])  # reset focus
            y = row + dy
            x = column + dx
            expected_idx = y * column_count + x
            if x < 0 or x >= column_count:
                expected_idx = None
            if y < 0 or y >= row_count:
                expected_idx = None
            if expected_idx is not None and expected_idx >= column_count * row_count:
                expected_idx = None
            if expected_idx is not None and expected_idx >= running_clients_num:
                if gapless_grid:
                    # last window fills remaining row
                    expected_idx = running_clients_num - 1
                    if expected_idx == start_idx:
                        # we only go there if this client
                        # is not the one where we started
                        expected_idx = None
                else:
                    expected_idx = None

            print(f"expected_idx = {expected_idx}")
            if expected_idx is None:
                hlwm.call_xfail(['focus', direction]) \
                    .expect_stderr('No neighbour found')
            else:
                hlwm.call(['focus', direction])
                assert hlwm.get_attr('clients.focus.winid') \
                    == running_clients[expected_idx]


@pytest.mark.parametrize("direction", ['left', 'right'])
def test_inner_neighbour_horizontal_layout(hlwm, direction):
    hlwm.create_clients(4)
    hlwm.call('set_layout horizontal')
    if direction == 'right':
        hlwm.call('focus_nth 0')
        expected_indices = [1, 2, 3]
    else:
        hlwm.call('focus_nth 3')
        expected_indices = [2, 1, 0]

    for i in expected_indices:
        hlwm.call(['focus', direction])
        assert int(hlwm.get_attr('tags.focus.curframe_windex')) == i

    hlwm.call_xfail(['focus', direction]) \
        .expect_stderr('No neighbour found')


@pytest.mark.parametrize('splittype,dir_work,dir_dummy', [
    ('horizontal', ('left', 'right'), ('up', 'down')),
    ('vertical', ('up', 'down'), ('left', 'right'))
])
def test_resize_flat_split(hlwm, splittype, dir_work, dir_dummy):
    # layout with a placeholder for the fraction
    layout_prefix = '(split ' + splittype + ':'
    layout_suffix = ':0 (clients grid:0) (clients grid:0))'
    layout_format = layout_prefix + '{}' + layout_suffix

    for i, signum in [(0, -1), (1, 1)]:
        hlwm.call(['load', layout_format.format('0.4')])
        hlwm.call(['resize', dir_work[i], '+0.1'])
        # extract the fraction:
        new_layout = hlwm.call('dump').stdout
        assert new_layout[0:len(layout_prefix)] == layout_prefix
        assert new_layout[-len(layout_suffix):] == layout_suffix
        new_fraction_str = new_layout[len(layout_prefix):-len(layout_suffix)]
        expected_fraction = Decimal('0.4') + Decimal(signum) * Decimal('0.1')
        assert new_fraction_str == str(expected_fraction)

    for d in dir_dummy:
        hlwm.call(['load', layout_format.format('0.4')])
        hlwm.call_xfail(['resize', d, '+0.1']) \
            .expect_stderr('No neighbour found')


@pytest.mark.parametrize('direction', ['left', 'right'])
def test_resize_default_delta(hlwm, direction):
    layout = '(split horizontal:0.4:1'
    layout += ' (clients vertical:0)'
    layout += ' (clients vertical:0)'
    layout += ')'
    hlwm.call(['load', layout])
    assert hlwm.call('dump').stdout == layout

    # resize
    hlwm.call(['resize', direction])
    assert hlwm.call('dump').stdout != layout

    # we expect the fraction to have changed by 0.02.
    # to verify this, we adjust the fraction by -0.02
    # and check that we're back at the original layout
    hlwm.call(['resize', direction, '-0.02'])
    assert hlwm.call('dump').stdout == layout


def test_resize_nested_split(hlwm):
    # a layout for a 2x2 grid of frames, where the
    # top left frame is focused
    layout = ' '.join([
        '(split horizontal:0.2:0',
        '(split vertical:0.3:0',
        '(clients grid:0) (clients grid:0))',
        '(split vertical:0.4:0',
        '(clients grid:0) (clients grid:0)))',
    ])

    # resize the root split
    for d, signum in [('left', -1), ('right', 1)]:
        hlwm.call(['load', layout])  # reset layout
        hlwm.call(['resize', d, '+0.05'])
        fraction = hlwm.call('dump').stdout.split(':')[1]
        expected = Decimal('0.2') + signum * Decimal('0.05')
        assert fraction == str(expected)

    # resize the nested split
    for d, signum in [('up', -1), ('down', 1)]:
        hlwm.call(['load', layout])  # reset layout
        hlwm.call(['resize', d, '+0.05'])
        fraction = hlwm.call('dump').stdout.split(':')[3]
        expected = Decimal('0.3') + signum * Decimal('0.05')
        assert fraction == str(expected)


@pytest.mark.parametrize('layout', [
    # layouts where existing windows are marked by W and the target
    # frame for the new window is marked by T
    '(clients max:0 T)',
    '(clients max:0 W T)',
    '(split vertical:0.5:1 (clients max:0 W) (clients max:0 W T))',
    # the selected frame isn't the first empty frame in order but
    # is perfectly fine, because it's empty:
    '(split vertical:0.5:1 (clients max:0) (clients max:0 T))',
    '(split vertical:0.5:0 (clients max:0 T) (clients max:0 W))',
    '(split horizontal:0.5:0 (split vertical:0.5:0 \
        (clients max:0 W) (clients max:0 T)) (clients grid:0))',
    '(split horizontal:0.5:0 (split vertical:0.5:0 \
        (clients max:0 T) (clients max:0)) (clients grid:0))',
    '(split horizontal:0.5:0 (split vertical:0.5:0 \
        (clients max:0 W) (clients max:0 T)) (clients grid:0))',
    '(split horizontal:0.5:0 (split vertical:0.5:0 \
        (clients max:0 W) (clients max:0 W)) (clients grid:0 T))',
    # here, no frames are empty, so the focused frame is taken
    '(split horizontal:0.5:0 (split vertical:0.5:1 \
        (clients max:0 W) (clients max:0 W T)) (clients grid:0 W))',
    # let geometry come into play:
    # +-----+-----+
    # |+-+-+|     |  here, the new window should go to (T)
    # || |T||  W  |  even though it's neighbour frame comes first
    # |+-+-+|     |  when traversing the tree
    # +-----+-----+
    '(split horizontal:0.5:1 \
        (split horizontal:0.5:0 (clients max:0) (clients max:0 T)) \
        (clients max:0 W))',
    #
    # +---+---+
    # |Foc|W| |   <- the new window should go to T, even though
    # | W +-+-+      the top right frame comes first when traversing
    # |   | T |      the layout tree.
    # +---+---+
    #
    '(split horizontal:0.5:0 \
        (clients vertical:0 W) \
        (split vertical:0.5:1 \
            (split horizontal:0.5:1 (clients vertical:0 W) \
                                    (clients vertical:0)) \
            (clients vertical:0 T)))',
])
def test_index_empty_frame_global(hlwm, layout):
    layout = re.sub(r' [ ]*', ' ', layout)
    for c in layout:
        if c != 'W':
            continue
        winid, _ = hlwm.create_client()
        layout = layout.replace('W', winid, 1)
    hlwm.call(['load', layout.replace('T', '')])
    hlwm.call('rule focus=off')
    hlwm.call('rule index=e')

    # create a new client, and expect it to go to the empty frame
    winid, _ = hlwm.create_client()

    assert layout.replace('T', winid) == hlwm.call('dump').stdout


def test_index_empty_frame_subtree(hlwm):
    layout = '(split horizontal:0.5:0 (clients max:0) \
        (split horizontal:0.5:0 (clients max:0 T) (clients max:0)))'
    layout = re.sub(r' [ ]*', ' ', layout)
    hlwm.call(['load', layout.replace('T', '')])
    hlwm.call('rule focus=off')
    hlwm.call('rule index=1e')

    # create a new client, and expect it to go to the empty frame
    # in the right subtree
    winid, _ = hlwm.create_client()

    assert layout.replace('T', winid) == hlwm.call('dump').stdout


@pytest.mark.parametrize("setting", [True, False])
@pytest.mark.parametrize("other_mon_exists", [True, False])
def test_focus_other_monitor(hlwm, other_mon_exists, setting):
    hlwm.call(['set', 'focus_crosses_monitor_boundaries', hlwm.bool(setting)])
    hlwm.call('add othertag')
    if other_mon_exists:
        hlwm.call('add_monitor 800x600+800+0')
    assert hlwm.get_attr('monitors.focus.index') == '0'

    if setting and other_mon_exists:
        hlwm.call('focus right')
        assert hlwm.get_attr('monitors.focus.index') == '1'
    else:
        hlwm.call_xfail('focus right') \
            .expect_stderr('No neighbour found')


def test_set_layout_invalid_layout_name(hlwm):
    hlwm.call_xfail('set_layout foobar') \
        .expect_stderr('set_layout: Invalid layout name: "foobar"')


def test_focus_edge(hlwm):
    hlwm.call('set focus_crosses_monitor_boundaries on')
    hlwm.call('add otherTag')
    hlwm.call('add_monitor 800x600+800+0')
    hlwm.call('split right')
    hlwm.call('split right')

    # we're on the leftmost frame
    layout_before = hlwm.call('dump').stdout
    hlwm.call('focus_edge left')
    assert layout_before == hlwm.call('dump').stdout
    assert hlwm.get_attr('monitors.focus.index') == '0'

    # focus_edge goes to the rightmost frame
    hlwm.call('focus_edge right')
    # we're still on the first monitor
    assert hlwm.get_attr('monitors.focus.index') == '0'
    # but right-most frame means, if we go right once more, we're on
    # the other monitor:
    hlwm.call('focus right')
    assert hlwm.get_attr('monitors.focus.index') == '1'
    assert hlwm.get_attr('settings.focus_crosses_monitor_boundaries') == 'true'


def test_tree_style_utf8(hlwm):
    # the following also tests utf8_string_at()
    hlwm.call(['set', 'tree_style', '╾│…├╰╼─╮'])
    hlwm.call('split top')
    hlwm.call('split vertical')

    assert hlwm.call('layout').stdout == textwrap.dedent("""\
    ╾─╮ vertical 50% selection=1
      ├─╼ vertical:
      ╰─╮ vertical 50% selection=0
      … ├─╼ vertical: [FOCUS]
      … ╰─╼ vertical:
    """)


def test_split_invalid_argument(hlwm):
    # this also tests all exceptions in Converter<FixPrecDec>::parse()
    wrongDecimal = [
        ('0.0f', "After '.' only digits"),
        ('0.+8', "After '.' only digits"),
        ('0.-8', "After '.' only digits"),
        ('0..0', "A decimal must have at most one '.'"),
        ('.3',   "There must be at least one digit"),
        ('.',    "There must be at least one digit"),
        ('b',    "stoi"),
        ('-.3',  "stoi"),
        ('+.8',  "stoi"),
    ]
    for d, msg in wrongDecimal:
        hlwm.call_xfail(['split', 'top', d]) \
            .expect_stderr('invalid argument: ' + msg)


def test_split_clamp_argument_smaller(hlwm):
    for d in ['-1.2', '-12', '0.05', '-0.05']:
        hlwm.call(['load', '(clients vertical:0)'])
        hlwm.call(['split', 'left', d])
        assert hlwm.call('dump').stdout == \
            '(split horizontal:0.1:1 (clients vertical:0) (clients vertical:0))'


def test_split_clamp_argument_bigger(hlwm):
    for d in ['1.2', '12', '0.95', '1']:
        hlwm.call(['load', '(clients vertical:0)'])
        hlwm.call(['split', 'left', d])
        assert hlwm.call('dump').stdout == \
            '(split horizontal:0.9:1 (clients vertical:0) (clients vertical:0))'


def test_resize_invalid_argument(hlwm):
    hlwm.call_xfail('resize left foo') \
        .expect_stderr('resize: ')


def test_resize_delta(hlwm):
    values = [
        # before, direction, delta, after
        ('0.2', 'left', '0.15', '0.1'),  # clamp to lower bound
        ('0.2', 'right', '0.15', '0.35'),
        ('0.7', 'right', '0.19', '0.89'),
        ('0.8', 'right', '0.19', '0.9'),  # clamp to upper bound
        ('0.8', 'left', '0.1989', '0.6011'),
        ('0.1', 'right', '0.2', '0.3'),
    ]
    for before, direction, delta, after in values:
        layout = '(split horizontal:{}:1'  # placeholder
        layout += ' (clients vertical:0) (clients vertical:0))'
        hlwm.call(['load', layout.format(before)])
        hlwm.call(['resize', direction, delta])
        assert hlwm.call('dump').stdout == layout.format(after)


def test_resize_cumulative(hlwm):
    layout = '(split horizontal:{}:1'  # placeholder
    layout += ' (clients vertical:0) (clients vertical:0))'
    hlwm.call(['load', layout.format(0.1)])
    for i in range(0, 35):
        hlwm.call(['resize', 'right', '0.02'])
    # 0.1 + 35 * 0.02 = 0.8
    assert hlwm.call('dump').stdout == layout.format('0.8')


def test_resize_clamp_argument_bigger(hlwm):
    layout = '(split horizontal:{}:1'  # placeholder
    layout += ' (clients vertical:0) (clients vertical:0))'
    hlwm.call(['load', layout.format('0.2')])
    hlwm.call('resize left 0.15')
    assert hlwm.call('dump').stdout == layout.format('0.1')
