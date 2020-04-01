import pytest
import re
import subprocess


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

    lines = [l.strip() for l in layout_str.splitlines()]
    # count the client frames
    assert len([l for l in lines if l[0] == 'C']) \
        == int(hlwm.get_attr('tags.0.frame_count'))
    # count the split frames
    assert len([l for l in lines if l[0] == 'S']) == num_splits
    # transform dump-output to something similar
    dumped = hlwm.call('dump').stdout \
        .replace('(', '\n').replace(')', '').strip() \
        .replace('split', 'S').replace('clients', 'C')
    # remove all information except for align/layout names
    dumped = re.sub(r':[^ ]*', '', dumped)
    dumped = re.sub(r'[ ]*\n', '\n', dumped)
    lines = [re.sub(r' [0-9]+% selection=[01]', '', l) for l in lines]
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


@pytest.mark.parametrize("running_clients_num", [0, 2])
@pytest.mark.parametrize("align", ["horizontal", "vertical"])
@pytest.mark.parametrize("fraction", ["0.1", "0.5", "0.7"])
def test_split_simple(hlwm, running_clients, align, fraction):
    hlwm.call(['split', align, fraction])
    assert hlwm.call('dump').stdout == \
        '(split {}:{}:0 (clients vertical:0{}) (clients vertical:0))' \
        .format(align, fraction, ''.join([' ' + c for c in running_clients]))


@pytest.mark.parametrize("align", ["horizontal", "vertical"])
def test_split_and_remove_with_smart_frame_surroundings(hlwm, x11, align):
    # Split frame, then merge it again to one root frame
    # Root frame should have no frame gaps in the end
    hlwm.call('set smart_frame_surroundings on')
    hlwm.call(['split', align])
    hlwm.call('remove')

    # Search for all frames, there should only be one
    frame_win_id = subprocess.run(['xdotool', 'search', '--class', '_HERBST_FRAME'],
                                  stdout=subprocess.PIPE,
                                  universal_newlines=True,
                                  check=True)
    frame_x11 = x11.window(frame_win_id.stdout)
    frame_geom = frame_x11.get_geometry()
    assert (frame_geom.width, frame_geom.height) == (800, 600)
