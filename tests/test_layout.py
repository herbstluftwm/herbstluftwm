import pytest
import re

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
    # TODO: reasonably handle focus, e.g. to have
    #assert hlwm.get_attr('tags.0.curframe_windex') == '2'


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

@pytest.mark.parametrize("running_clients_num", [0,1,5])
@pytest.mark.parametrize("index", [0,1,3,5])
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
    for i in range(0,4):
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

