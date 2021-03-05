import pytest
import re
import math
import textwrap
from decimal import Decimal


def normalize_layout_string(layout_string):
    """normalize a layout string such that it is characterwise identical
    to the output of the 'dump' command
    """
    layout_string = layout_string.replace('\n', ' ')
    # drop multiple whitespace
    layout_string = re.sub(r'[ ]+', ' ', layout_string)
    # drop whitespace after opening parenthesis
    layout_string = re.sub(r'[(] ', r'(', layout_string)
    # drop whitespace and before closing parenthesis
    layout_string = re.sub(r' [)]', r')', layout_string)
    return layout_string.strip()


def verify_frame_objects_via_dump(hlwm, tag_object="tags.focus"):
    """verify, that the frame objects for the given tag_object match
    with the output of the 'dump' command this tag"""
    def construct_layout_descr(hlwm, frame_object_path):
        """construct the layout description from the object tree where
        every window id is only present as a placeholder called 'WINID'
        """
        p = frame_object_path  # just to make the next definition lighter
        # write the formatted string to an attribute whose path is 'DUMP'
        cmd = [
            'mktemp', 'string', 'DUMP', 'chain'
        ]
        # we run the sprintf commands for both the 'split' and the 'leaf'
        # case and we will know that one of them will succeed, because every
        # frame is one of them
        split_sprintf = [
            'sprintf', 'S', 'split %s:%s:%s', p + '.split_type',
            p + '.fraction', p + '.selection',
            'set_attr', 'DUMP', 'S'
        ]
        leaf_sprintf = [
            'sprintf', 'L', 'clients %s:%s', p + '.algorithm', p + '.selection',
            'set_attr', 'DUMP', 'L'
        ]
        # ... we simply run both
        cmd += ['//', 'silent'] + split_sprintf
        cmd += ['//', 'silent'] + leaf_sprintf
        # ... and we know that one of them must have filled the attribute
        # with path 'DUMP'. Fun fact: this has a slight flavor of the
        # 'universal property of the coproduct'
        cmd += ['//', 'get_attr', 'DUMP']
        layout_desc = hlwm.call(cmd).stdout
        if layout_desc[0] == 's':
            child0 = construct_layout_descr(hlwm, frame_object_path + '.0')
            child1 = construct_layout_descr(hlwm, frame_object_path + '.1')
            return '({} {} {})'.format(layout_desc, child0, child1)
        else:
            assert layout_desc[0] == 'c'
            hlwm.call(['attr', frame_object_path])
            client_count_str = hlwm.get_attr(frame_object_path + '.client_count')
            return '(' + layout_desc + int(client_count_str) * ' WINID' + ')'

    layout_desc = construct_layout_descr(hlwm, tag_object + '.tiling.root')
    dump = hlwm.call(f'substitute NAME {tag_object}.name dump NAME').stdout
    # replace window IDs by the placeholder
    dump = re.sub('0x[0-9A-Za-z]*', 'WINID', dump)
    assert dump == layout_desc


@pytest.mark.parametrize("running_clients_num", [0, 1, 2])
def test_single_frame_layout(hlwm, running_clients, running_clients_num):
    assert hlwm.get_attr('tags.0.curframe_windex') == '0'
    assert int(hlwm.get_attr('tags.0.client_count')) == running_clients_num
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == running_clients_num
    verify_frame_objects_via_dump(hlwm)


def test_single_frame_layout_three(hlwm):
    hlwm.create_clients(3)
    assert hlwm.get_attr('tags.0.curframe_windex') == '0'
    assert int(hlwm.get_attr('tags.0.client_count')) == 3
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == 3
    verify_frame_objects_via_dump(hlwm)


@pytest.mark.parametrize("running_clients_num", [2, 3])
def test_explode(hlwm, running_clients, running_clients_num):
    assert running_clients_num >= 2, "explode behaves as auto for one client"
    hlwm.call('split explode')
    assert hlwm.get_attr('tags.0.curframe_windex') == '0'
    number_upper = (running_clients_num + 1) // 2
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == number_upper
    assert int(hlwm.get_attr('tags.0.client_count')) == running_clients_num
    assert hlwm.get_attr('tags.0.frame_count') == '2'
    verify_frame_objects_via_dump(hlwm)


def test_explode_second_client(hlwm):
    winids = hlwm.create_clients(2)
    hlwm.call(['load', f'(clients vertical:1 {winids[0]} {winids[1]})'])
    hlwm.call('split explode')

    assert hlwm.get_attr('tags.0.curframe_wcount') == '1'
    assert hlwm.get_attr('tags.0.curframe_windex') == '0'
    expected_layout = normalize_layout_string(f"""\
    (split vertical:0.5:1
    (clients vertical:0 {winids[0]})
    (clients vertical:0 {winids[1]}))
    """)
    assert hlwm.call('dump').stdout == expected_layout
    verify_frame_objects_via_dump(hlwm)


@pytest.mark.parametrize("running_clients_num", [4])
@pytest.mark.parametrize("focus_idx", range(0, 4))
def test_explode_preserves_focus(hlwm, running_clients, running_clients_num, focus_idx):
    hlwm.call(['focus_nth', focus_idx])
    winid = hlwm.attr.clients.focus.winid()

    for i in range(0, running_clients_num):
        hlwm.call('split explode')
        assert winid == hlwm.attr.clients.focus.winid()


@pytest.mark.parametrize("running_clients_num", [0, 1, 4])
def test_remove(hlwm, running_clients, running_clients_num):
    hlwm.call('split explode')
    hlwm.call('remove')
    assert int(hlwm.get_attr('tags.0.curframe_wcount')) == running_clients_num
    assert int(hlwm.get_attr('tags.0.client_count')) == running_clients_num
    assert hlwm.get_attr('tags.0.frame_count') == '1'
    verify_frame_objects_via_dump(hlwm)


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


@pytest.mark.parametrize("mode", ['setting', 'flag', 'flag-double', 'flag-shadow'])
@pytest.mark.parametrize("setting_value", [True, False])
@pytest.mark.parametrize("external_only", [True, False])
@pytest.mark.parametrize("running_clients_num", [3])
def test_focus_internal_external(hlwm, mode, setting_value, external_only, running_clients):
    hlwm.call(f'set default_direction_external_only {hlwm.bool(setting_value)}')
    layout = f"""
    (split vertical:0.5:0
        (clients vertical:0 {running_clients[0]} {running_clients[1]})
        (clients vertical:0 {running_clients[2]}))
    """.replace('\n', ' ').strip()
    hlwm.call(['load', layout])
    assert hlwm.get_attr('clients.focus.winid') == running_clients[0]

    # we will now run 'focus' 'down' with -i or -e
    cmd = ['focus']
    if mode == 'setting':
        hlwm.call(f'set default_direction_external_only {hlwm.bool(external_only)}')
    else:
        # mode is one of the flag*-modes
        extonly2flag = {
            True: '-e',
            False: '-i',
        }
        if mode == 'flag-shadow':
            # something like: focus -i -e down
            # Here, an earlier and contradictory flag gets shadowed
            cmd.append(extonly2flag[not external_only])
        if mode == 'flag-double':
            cmd.append(extonly2flag[external_only])
        # the significant command line flag:
        cmd.append(extonly2flag[external_only])
    cmd += ['down']
    hlwm.call(cmd)

    if external_only:
        expected_new_focus = running_clients[2]
    else:
        expected_new_focus = running_clients[1]
    assert hlwm.get_attr('clients.focus.winid') == expected_new_focus


def test_argparse_invalid_flag(hlwm):
    # here, '-v' is not a valid flag, so it is assumed
    # to be the first positional argument
    hlwm.call_xfail('focus -v down') \
        .expect_stderr('Cannot parse argument "-v"')
    # in the following, the positional argument has been
    # parsed already, so the error message is different:
    hlwm.call_xfail('focus down -v') \
        .expect_stderr('Unknown argument or flag "-v"')
    hlwm.call_xfail('focus down -v -i') \
        .expect_stderr('Unknown argument or flag "-v"')
    hlwm.call_xfail('focus down -i -v') \
        .expect_stderr('Unknown argument or flag "-v"')


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
    layout['0'] = '(split vertical:0.7:1 {} {})'.format(
        layout['00'], layout['01'])
    layout['1'] = '(clients horizontal:0)'
    layout[''] = '(split horizontal:0.65:0 {} {})'.format(
        layout['0'], layout['1'])
    hlwm.call(['load', layout['']])

    # also test more specific frame indices:
    frame_index_aliases = [
        ('@', '01'),
        ('@p', '0'),
        ('@p/', '00'),
        ('@p/', '00'),
        ('@pp', ''),
        ('@ppp', ''),  # going up too much does not exceed the root
        ('..', '01'),
        ('...', '01'),
        ('./', '00'),
    ]
    for complicated, normalized in frame_index_aliases:
        layout[complicated] = layout[normalized]

    # test all the frame tree portions in the dict 'layout':
    tag = hlwm.get_attr('tags.focus.name')
    for index in layout.keys():
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
    # check that the right winid is focused
    layout = hlwm.call(['dump', '', '@']).stdout
    winids = re.findall('0x[a-fA-f0-9]*', layout)
    if len(winids) > 0:
        assert winids[new_windex] == hlwm.get_attr('clients.focus.winid')


@pytest.mark.parametrize("running_clients_num", [0, 1, 5])
@pytest.mark.parametrize("index", [0, 1, 3, 5])
def test_focus_nth(hlwm, running_clients, running_clients_num, index):
    # bring the clients in the right order
    layout = '(clients vertical:0 '
    layout += ' '.join(running_clients)
    layout += ')'
    hlwm.call(['load', layout])

    # focus the n_th
    hlwm.call('focus_nth {}'.format(index))

    windex = int(hlwm.get_attr('tags.0.curframe_windex'))
    assert windex == max(0, min(index, running_clients_num - 1))
    if running_clients_num > 0:
        assert hlwm.get_attr('clients.focus.winid') == running_clients[windex]


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


def test_cycle_all_errors(hlwm):
    hlwm.call_xfail('cycle_all 1 2') \
        .expect_stderr('Unknown argument or flag "2"')
    hlwm.call_xfail('cycle_all -3') \
        .expect_stderr('argument out of range')
    hlwm.call_xfail('cycle_all 3') \
        .expect_stderr('argument out of range')
    hlwm.call_xfail('cycle_all -s 1') \
        .expect_stderr('Cannot.*"-s"')
    hlwm.call_xfail('cycle_all --skip-invisible -s 1') \
        .expect_stderr('Cannot.*"-s"')
    hlwm.call_xfail('cycle_all --skip-invisible -s 1') \
        .expect_stderr('Cannot.*"-s"')
    hlwm.call_xfail('cycle_all 1 2 3') \
        .expect_stderr('Unknown argument.*2')
    hlwm.call_xfail('cycle_all 1 2 --skip-invisible') \
        .expect_stderr('Unknown argument.*2')
    hlwm.call_xfail('cycle_all 1 --skip-invisible 3') \
        .expect_stderr('Unknown argument.*3')


def test_cycle_all_optionality(hlwm):
    # on the other hand, the error handling should not be triggered
    # by the following:
    hlwm.call('split explode')
    hlwm.call('split explode')

    def layout(focus1, focus2):
        return normalize_layout_string(f"""
            (split horizontal:0.5:{focus1}
                (clients max:0)
                (split vertical:0.5:{focus2}
                    (clients max:0)
                    (clients max:0)))
        """)

    # on the above layout, the following args must have
    # identical results
    for args in [[], ['--skip-invisible', '+1'], ['+1', '--skip-invisible'], ['+1']]:
        hlwm.call(['load', layout(0, 1)])
        hlwm.call(['cycle_all'] + args)
        assert hlwm.call('dump').stdout == layout(1, 0)


def test_cycle_all_completion(hlwm):
    assert hlwm.complete(['cycle_all']) == ['+1', '--skip-invisible', '-1']
    assert hlwm.complete(['cycle_all', '--skip-invisible']) == ['+1', '-1']
    assert hlwm.complete(['cycle_all', '-1']) == ['--skip-invisible']
    hlwm.command_has_all_args(['cycle_all', '-1', '--skip-invisible'])
    hlwm.command_has_all_args(['cycle_all', '--skip-invisible', '+1'])
    # passing too many arguments still results in no completions:
    hlwm.command_has_all_args(['cycle_all', '1', '2', '3'])
    hlwm.command_has_all_args(['cycle_all', '1', '2', '3', '4'])
    hlwm.command_has_all_args(['cycle_all', '1', '2', '3', '4', '5'])


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


@pytest.mark.parametrize("delta", [1, -1])
@pytest.mark.parametrize("clients_minimized", [
    # every entry here is a list that determines the number
    # of floating clients and which of those are minimized.
    [],  # no floating clients at all
    [True],  # only one, but it's minimized
    [False],  # also covered by test_cycle_all_with_floating_clients
    [False, True, False],  # one minimized in the middle
    [True, True],
    [False, True, True, False],
    [True, True, True],
    [True, False, True, False, True],
    [False, False, True],
    [True, False, False],
    # multiple skips before/after wrapping between floating and tiling layer:
    [True, True, False, False],
    [False, False, True, True],
])
def test_cycle_all_skips_minimized(hlwm, delta, clients_minimized):
    # create one tiled client and a list of floating clients,
    # some of which are minimized.
    tiled_client, _ = hlwm.create_client()
    hlwm.call('rule floating=on')
    non_minimized_clients = []
    for minimized in clients_minimized:
        winid, _ = hlwm.create_client()
        hlwm.call(f'set_attr clients.{winid}.minimized {hlwm.bool(minimized)}')
        if not minimized:
            non_minimized_clients.append(winid)
    if delta == -1:
        non_minimized_clients = list(reversed(non_minimized_clients))
    expected_winids = non_minimized_clients + [tiled_client]

    assert hlwm.get_attr('clients.focus.winid') == tiled_client
    print("expecting the clients in the order {}".format(' '.join(expected_winids)))
    for expected in expected_winids:
        # run 'cycle_all' as often as specified by 'expected_winids'.
        hlwm.call(['cycle_all', delta])

        # since there is a tiled client, clients.focus must always exist.
        # we check that the focused client must never
        # be minimized and must always be visible
        assert hlwm.get_attr('clients.focus.minimized') == 'false'
        assert hlwm.get_attr('clients.focus.visible') == 'true'
        assert hlwm.get_attr('clients.focus.winid') == expected


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


def test_smart_window_surroundings_urgent(hlwm, x11):
    hlwm.call('set smart_window_surroundings on')
    hlwm.call('set smart_frame_surroundings on')
    hlwm.call('attr theme.border_width 42')
    hlwm.call('attr theme.minimal.border_width 0')

    win1, win1_id = x11.create_client()

    # With only one client and smart_window_surroundings enabled,
    # no gaps or surroundings are applied and
    # the minimal decoration scheme is used
    geo1 = win1.get_geometry()
    assert (geo1.x, geo1.y) == (0, 0)

    # Move it to another tag and make it urgent
    hlwm.call('add otherTag')
    hlwm.call('move otherTag')
    x11.make_window_urgent(win1)
    assert hlwm.get_attr(f'clients.{win1_id}.urgent') == 'true'

    # The minimal theme should still apply!
    geo1 = win1.get_geometry()
    assert (geo1.x, geo1.y) == (0, 0)


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
        .expect_stderr('Cannot.* "foobar":.*one of: vertical, horizontal')


@pytest.mark.parametrize("command", ['focus_edge', 'shift_edge'])
def test_focus_edge_shift_edge(hlwm, command):
    hlwm.call('set focus_crosses_monitor_boundaries on')
    hlwm.call('add otherTag')
    hlwm.call('add_monitor 800x600+800+0')
    hlwm.call('split right')
    hlwm.call('split right')

    if command == 'shift_edge':
        # the only difference between 'shift_edge' and 'focus_edge'
        # is that we take the focused window with us
        hlwm.create_client()

    # we're on the leftmost frame
    layout_before = hlwm.call('dump').stdout
    hlwm.call([command, 'left'])
    assert hlwm.attr.tags.focus.tiling.focused_frame.index() == '00'
    assert layout_before == hlwm.call('dump').stdout
    assert hlwm.get_attr('monitors.focus.index') == '0'

    # focus_edge/shift_edge goes to the rightmost frame
    hlwm.call([command, 'right'])
    # we're still on the first monitor
    assert hlwm.get_attr('monitors.focus.index') == '0'
    # but in the right-most frame
    assert hlwm.attr.tags.focus.tiling.focused_frame.index() == '1'
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
        ('.3', "There must be at least one digit"),
        ('.', "There must be at least one digit"),
        ('b', "stoi"),
        ('-.3', "stoi"),
        ('+.8', "stoi"),
        ('', "Decimal is empty"),
    ]
    for d, msg in wrongDecimal:
        hlwm.call_xfail(['split', 'top', d]) \
            .expect_stderr('Cannot parse argument \".*\": {}'.format(msg))


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


def current_layout_name(hlwm):
    proc = hlwm.call(['dump', '', '@'])
    m = re.match(r'^\([^ ]* ([^:]*):.*\)$', proc.stdout)
    return m.group(1)


@pytest.mark.parametrize("delta", ['-1', '+1'])
def test_cycle_layout_name_not_in_list(hlwm, delta):
    hlwm.call('set_layout vertical')

    # if the current name is not contained in the custom list
    # of layouts, then the first entry in the custom list
    # has to be picked
    hlwm.call(f'cycle_layout {delta} max grid')

    assert current_layout_name(hlwm) == 'max'


@pytest.mark.parametrize("delta", ['-1', '+1'])
def test_cycle_layout_name_in_the_list(hlwm, delta):
    hlwm.call('set_layout vertical')
    layouts = ['vertical', 'max', 'grid']
    # here, 'layouts' does not contain 'horizontal'. By this, we
    # implicitly verify that we never reach the 'horizontal' layout
    # since the 'expected_layouts' list also does not contain 'horizontal'
    cycle_layout_cmd = ['cycle_layout', delta] + layouts
    # expected list of layouts after calling
    # 'cycle_layout_cmd' multiple times:
    if delta == '+1':
        expected_layouts = layouts[1:] + layouts
    elif delta == '-1':
        expected_layouts = reversed(layouts + layouts)

    for expected in expected_layouts:
        hlwm.call(cycle_layout_cmd)
        assert current_layout_name(hlwm) == expected


@pytest.mark.parametrize("splitmode,context", [
    ('top', '(split vertical:0.5:1 (clients vertical:0) {})'),
    ('bottom', '(split vertical:0.5:0 {} (clients vertical:0))'),
    ('left', '(split horizontal:0.5:1 (clients vertical:0) {})'),
    ('right', '(split horizontal:0.5:0 {} (clients vertical:0))'),
])
@pytest.mark.parametrize("oldlayout", [
    '(split horizontal:0.587:1 (clients vertical:0 W) (split vertical:0.4029:1 (clients max:0 W) (clients max:0)))',
])
def test_split_root_frame(hlwm, splitmode, context, oldlayout):
    for ch in str(oldlayout):  # duplicate the string here
        if ch != 'W':
            continue
        winid, _ = hlwm.create_client()
        oldlayout = oldlayout.replace('W', winid, 1)

    hlwm.call(['load', oldlayout])
    assert hlwm.call('dump').stdout == oldlayout

    # here, '' is the frame index of the root frame
    hlwm.call(['split', splitmode, '0.5', ''])

    hlwm.call('get default_frame_layout')
    assert context.format(oldlayout) == hlwm.call(['dump']).stdout


@pytest.mark.parametrize("splitmode,context", [
    ('top', '(split vertical:0.5:1 (clients vertical:0) {})'),
    ('bottom', '(split vertical:0.5:0 {} (clients vertical:0))'),
    ('left', '(split horizontal:0.5:1 (clients vertical:0) {})'),
    ('right', '(split horizontal:0.5:0 {} (clients vertical:0))'),
])
def test_split_something_in_between(hlwm, splitmode, context):
    outer_layer = '(split vertical:0.3:1 (clients max:0) {})'
    inner_layer = '(split horizontal:0.2:0 (clients max:0) (clients grid:0))'

    initial_layout = outer_layer.format(inner_layer)
    hlwm.call(['load', initial_layout])
    assert hlwm.call('dump').stdout == initial_layout

    # split the {} of outer_layer
    hlwm.call(['split', splitmode, '0.5', '1'])

    expected_layout = outer_layer.format(context.format(inner_layer))
    assert hlwm.call('dump').stdout == expected_layout


def test_frame_index_attribute(hlwm):
    # split the frames with the following indices
    split_index = [
        '', '0', '00', '1', '11', '111'
    ]

    def verify_frame_tree(object_path, index_prefix):
        assert hlwm.get_attr(f'{object_path}.index') == index_prefix
        if '0' in hlwm.list_children(object_path):
            verify_frame_tree(f'{object_path}.0', f'{index_prefix}0')
            verify_frame_tree(f'{object_path}.1', f'{index_prefix}1')

    for index in split_index:
        hlwm.call(['split', 'auto', '0.5', index])

        # after each splitting operation, check that
        # the frame's index attribute is correct:
        verify_frame_tree('tags.focus.tiling.root', '')


def test_focused_frame_child(hlwm):
    # do it as one test case to see that the
    # child correctly adjusts at run-time
    test_data = [
        ('', '(clients max:0)'),
        ('0', '(split vertical:0.5:0 (clients max:0) (clients max:0))'),
        ('1', '(split vertical:0.5:1 (clients max:0) (clients max:0))'),
        ('00', '(split vertical:0.5:0 (split vertical:0.5:0) (clients max:0))'),
        ('01', '(split vertical:0.5:0 (split vertical:0.5:1) (clients max:0))'),
    ]
    for focused_index, layout in test_data:
        hlwm.call(['load', layout])
        assert hlwm.get_attr('tags.0.tiling.focused_frame.index') == focused_index


def verify_frame_indices(hlwm, root='tags.focus.tiling.root', index=[]):
    """traverse all frame objects under the given 'root' frame object
    and verify that their 'index' attribute has the correct value according to
    their path in the frame tree.
    """
    path = root.rstrip('.') + '.' + '.'.join(index)
    assert hlwm.get_attr(path.rstrip('.') + '.index') == ''.join(index)
    if '0' in hlwm.list_children(path):
        # if 'path' is a frame split object
        verify_frame_indices(hlwm, root=root, index=index + ['0'])
        verify_frame_indices(hlwm, root=root, index=index + ['1'])


@pytest.mark.parametrize("old,new", [
    ('(split vertical:0.4:0 {a} {b})', '(split vertical:0.6:1 {b} {a})'),
    ('(split horizontal:0.3:0 {a} {b})', '(split horizontal:0.7:1 {b} {a})'),
    ('(split vertical:0.5:1 {a} {b})', '(split vertical:0.5:0 {b} {a})'),
    ('(split horizontal:0.2:1 {a} {b})', '(split horizontal:0.8:0 {b} {a})'),
])
@pytest.mark.parametrize("mode", ['horizontal', 'vertical'])
def test_mirror_horizontal_or_vertical_one_split(hlwm, old, new, mode):
    frame_a = '(clients horizontal:0)'
    frame_b = '(clients vertical:0)'
    hlwm.call(['load', old.format(a=frame_a, b=frame_b)])

    hlwm.call(f'mirror {mode}')

    if old.find(mode) < 0:
        expected = old.format(a=frame_a, b=frame_b)  # nothing changes
    else:
        expected = new.format(a=frame_a, b=frame_b)
    assert hlwm.call('dump').stdout == expected
    verify_frame_indices(hlwm)


@pytest.mark.parametrize("num_splits", [1, 2, 3, 4])
def test_mirror_vs_rotate(hlwm, num_splits):
    for _ in range(0, num_splits):
        hlwm.call('split explode 0.4')

    layout = hlwm.call('dump').stdout

    # compute the effect of rotating by 180 degrees
    hlwm.call('chain , rotate , rotate')
    verify_frame_indices(hlwm)
    layout_after_rotate = hlwm.call('dump').stdout
    # restore original layout
    hlwm.call(['load', layout])

    # rotating by 180 degrees is the same as
    # flipping both horizontally and vertically
    hlwm.call('mirror both')
    assert layout_after_rotate == hlwm.call('dump').stdout
    verify_frame_indices(hlwm)


@pytest.mark.parametrize("direction, frameindex", [
    ('left', '00'),
    ('right', '1'),
    ('up', '0100'),
    ('down', '011'),
])
@pytest.mark.parametrize("clients_per_frame", [0, 1, 2])  # client count in other frame
def test_shift_to_other_frame(hlwm, direction, frameindex, clients_per_frame):
    """
    in a frame grid with 3 columns, where the middle column has 3 rows, we put
    the focused window in the middle, and then invoke 'shift' with the given
    'direction'. Then, it is checked that the window stays focused but now
    resides in the frame with the given 'frameindex'
    """
    winid, _ = hlwm.create_client()

    def otherclients():
        # put 'otherclients'-many clients in every other frame
        winids = hlwm.create_clients(clients_per_frame)
        return ' '.join(winids)

    layout_131 = f"""
    (split horizontal:0.66:0
        (split horizontal:0.5:1
            (clients vertical:0 {otherclients()})
            (split vertical:0.66:0
                (split vertical:0.5:1
                    (clients vertical:0 {otherclients()})
                    (clients vertical:0 {winid}))
                (clients vertical:0 {otherclients()})))
        (clients vertical:0 {otherclients()}))
    """
    hlwm.call(['load', layout_131])
    assert hlwm.attr.clients.focus.winid() == winid
    assert hlwm.attr.tags.focus.tiling.focused_frame.index() == '0101'

    hlwm.call(['shift', direction])

    # the window is still focused
    assert hlwm.attr.clients.focus.winid() == winid
    # but it's now in another frame
    assert hlwm.attr.tags.focus.tiling.focused_frame.index() == frameindex


@pytest.mark.parametrize("flag", [None, '-i', '-e'])
@pytest.mark.parametrize("setting", ['on', 'off'])
@pytest.mark.parametrize("running_clients_num", [1, 2, 4])
def test_shift_internal_vs_external(hlwm, flag, setting, running_clients):
    winids = running_clients  # define a shorter name for the slicing later

    def layout(upper_winids, lower_winids, focus_idx):
        # 'focus_idx' is the index of the focused window in
        # the list 'upper_winids + lower_winids'
        if focus_idx < len(upper_winids):
            frame_selected = 0
            upper_idx = focus_idx
            lower_idx = 0
        else:
            frame_selected = 1
            upper_idx = 0
            lower_idx = focus_idx - len(upper_winids)
        return normalize_layout_string(f"""
        (split vertical:0.5:{frame_selected}
            (clients vertical:{upper_idx} {' '.join(upper_winids)})
            (clients vertical:{lower_idx} {' '.join(lower_winids)}))
        """)
    # put all clients in the upper frame
    hlwm.call(['load', layout(winids, [], 0)])
    assert hlwm.attr.clients.focus.winid() == winids[0]
    hlwm.attr.settings.default_direction_external_only = setting

    if flag == '-e' or (flag is None and setting == 'on') or len(winids) == 1:
        # client is moved do another frame
        expected_layout = layout(winids[1:], [winids[0]], len(winids[1:]))
    else:
        # client is moved within frame
        expected_layout = layout(winids[1:2] + [winids[0]] + winids[2:], [], 1)

    # shift the window
    cmd = ['shift']
    cmd += [flag] if flag is not None else []
    cmd += ['down']
    hlwm.call(cmd)

    # the same client is still focused
    assert hlwm.attr.clients.focus.winid() == winids[0]
    assert hlwm.call(['dump']).stdout == expected_layout


def test_shift_no_client_focused(hlwm):
    hlwm.call('split vertical')

    hlwm.call_xfail('shift down') \
        .expect_stderr('No client focused')


@pytest.mark.parametrize("split", [True, False])
def test_shift_no_neighbour_frame(hlwm, split):
    if split:
        # splitting 'vertical' does not make a difference
        # because it will focus the first frame
        hlwm.call('split vertical')

    hlwm.create_client()

    hlwm.call_xfail('shift up') \
        .expect_stderr('No neighbour found')


@pytest.mark.parametrize("cross_monitor_bounds", [True, False])
def test_shift_to_other_monitor_if_allowed_by_setting(hlwm, cross_monitor_bounds):
    hlwm.attr.settings.focus_crosses_monitor_boundaries = hlwm.bool(cross_monitor_bounds)
    hlwm.call('add othertag')
    # monitor 1 right of monitor 0
    hlwm.call('set_monitors 800x600+0+0 800x600+800+0')

    hlwm.call('rule focus=on switchtag=off')
    winid, _ = hlwm.create_client()
    # put another window on the other tag
    hlwm.call('rule tag=othertag')
    hlwm.create_client()
    # but the 'winid' is focused on monitor 0
    assert hlwm.attr.clients.focus.winid() == winid
    assert hlwm.attr.monitors.focus.index() == '0'

    command = ['shift', 'right']
    if cross_monitor_bounds:
        # 'winid' gets moved to the monitor on the right
        hlwm.call(command)
        assert hlwm.attr.monitors.focus.index() == '1'
    else:
        # the setting forbids that the window leaves the tag
        hlwm.call_xfail(command) \
            .expect_stderr("No neighbour found")
    # in any case 'winid' is still focused
    assert hlwm.attr.clients.focus.winid() == winid


@pytest.mark.parametrize("floating", ['off', 'tag', 'window'])
def test_shift_to_other_monitor_floating(hlwm, floating):
    hlwm.call('add othertag')
    # monitor 1 left of monitor 0
    hlwm.call('set_monitors 800x600+800+0 800x600+0+0')
    snap_gap = 8
    hlwm.attr.settings.snap_gap = snap_gap
    winid, _ = hlwm.create_client(position=(snap_gap, snap_gap))
    if floating == 'tag':
        hlwm.attr.tags[0].floating = 'on'
    elif floating == 'window':
        hlwm.attr.clients[winid].floating = 'on'
    else:
        assert floating == 'off'
    assert hlwm.attr.clients.focus.winid() == winid
    assert hlwm.attr.monitors.focus.index() == '0'

    hlwm.call('shift left')

    assert hlwm.attr.clients.focus.winid() == winid
    assert hlwm.attr.monitors.focus.index() == '1'


@pytest.mark.parametrize("floating", [True, False])
def test_shift_stays_on_monitor(hlwm, floating):
    hlwm.call('add othertag')
    # monitor 1 below monitor 0
    hlwm.call('set_monitors 800x600+0+0 800x600+0+800')
    if floating:
        hlwm.attr.tags.focus.floating = 'on'
    else:
        # create empty frame at the bottom
        hlwm.call('split bottom')

    winid, _ = hlwm.create_client(position=(0, 0))
    assert hlwm.attr.clients.focus.winid() == winid
    assert hlwm.attr.monitors.focus.index() == '0'

    # the new client is far away from the bottom edge of monitor 0.
    # so shifting it downwards makes it stay on monitor 0
    hlwm.call('shift down')

    assert hlwm.attr.clients.focus.winid() == winid
    assert hlwm.attr.monitors.focus.index() == '0'

    # now the client is on the bottom corner of the monitor.
    # so shifting it again will make it enter monitor 1
    hlwm.call('shift down')

    assert hlwm.attr.clients.focus.winid() == winid
    assert hlwm.attr.monitors.focus.index() == '1'


def test_shift_no_monitor_in_direction(hlwm):
    hlwm.call('add othertag')
    # monitor 1 below monitor 0
    hlwm.call('set_monitors 800x600+0+0 800x600+0+800')
    winid, _ = hlwm.create_client()

    for direction in ['left', 'right', 'up']:
        hlwm.call_xfail(['shift', direction]) \
            .expect_stderr('No neighbour found')
        assert hlwm.attr.clients.focus.winid() == winid


def test_focus_shift_completion(hlwm):
    for cmd in ['shift', 'focus']:
        directions = ['down', 'up', 'left', 'right']
        flags = ['-i', '-e']
        assert sorted(directions + flags) == hlwm.complete([cmd])

        assert sorted(flags) == hlwm.complete([cmd, 'down'])

        assert sorted(directions + ['-i']) == hlwm.complete([cmd, '-e'])

        # actually, passing both -i and -e makes no sense,
        # but ArgParse does not know that the flags exclude each other
        hlwm.command_has_all_args([cmd, 'down', '-i', '-e'])


def test_frame_leaf_selection_change(hlwm):
    """test the attribute FrameLeaf::selection"""
    clients = hlwm.create_clients(3)

    def layout(idx):
        return f"(clients vertical:{idx} {clients[0]} {clients[1]} {clients[2]})"

    hlwm.call(['load', layout(0)])

    for i in range(0, 3):
        hlwm.attr.tags.focus.tiling.focused_frame.selection = i
        assert hlwm.attr.clients.focus.winid() == clients[i]
        assert hlwm.call('dump').stdout == layout(i)


def test_frame_leaf_selection_if_empty(hlwm):
    assert hlwm.attr.tags.focus.tiling.root.selection() == '0'
    hlwm.attr.tags.focus.tiling.root.selection = 0
    assert hlwm.attr.tags.focus.tiling.root.selection() == '0'

    hlwm.call_xfail('attr tags.focus.tiling.root.selection 1') \
        .expect_stderr('out of range')

    hlwm.call_xfail('attr tags.focus.tiling.root.selection 2') \
        .expect_stderr('out of range')

    hlwm.call_xfail('attr tags.focus.tiling.root.selection -1') \
        .expect_stderr('out of range')


def test_frame_split_selection_change(hlwm):
    """test the attribute FrameSplit::selection"""
    clients = hlwm.create_clients(2)

    def layout(idx):
        return normalize_layout_string(f"""
            (split horizontal:0.5:{idx}
                (clients vertical:0 {clients[0]})
                (clients vertical:0 {clients[1]}))
        """)

    hlwm.call(['load', layout(0)])

    for i in [0, 1]:
        hlwm.attr.tags.focus.tiling.root.selection = i
        assert hlwm.attr.clients.focus.winid() == clients[i]
        assert hlwm.call('dump').stdout == layout(i)


def test_frame_selection_invalid_arg(hlwm):
    hlwm.create_clients(6)  # 6 clients
    hlwm.call('split explode')  # 2 frames, so 3 clients per frame
    hlwm.call('dump')

    # for frame splits, the selection can be 0 or 1
    hlwm.call('attr tags.focus.tiling.root.selection 0')  # no failure
    hlwm.call('attr tags.focus.tiling.root.selection 1')  # no failure
    hlwm.call_xfail('attr tags.focus.tiling.root.selection -1') \
        .expect_stderr('out of range')
    hlwm.call_xfail('attr tags.focus.tiling.root.selection 2') \
        .expect_stderr('out of range')

    # for frame leafs, the selection only must be lower than the client count
    hlwm.call('attr tags.focus.tiling.focused_frame.selection 0')  # no failure
    hlwm.call('attr tags.focus.tiling.focused_frame.selection 1')  # no failure
    hlwm.call('attr tags.focus.tiling.focused_frame.selection 2')  # no failure
    hlwm.call_xfail('attr tags.focus.tiling.focused_frame.selection -1') \
        .expect_stderr('out of range')
    hlwm.call_xfail('attr tags.focus.tiling.focused_frame.selection 3') \
        .expect_stderr('out of range')


def test_frame_split_attribute_size_changes(hlwm, x11):
    """
    Test that changing the attributes 'fraction' or 'split_type' of FrameSplit
    affects window sizes
    """
    # two clients side by side:
    win1, winid1 = x11.create_client()
    win2, winid2 = x11.create_client()
    layout = f'(split horizontal:0.5:0 (clients max:0 {winid1}) (clients max:0 {winid2}))'
    layout = normalize_layout_string(layout)
    for attr, value in [('fraction', '0.6'), ('split_type', 'vertical')]:
        hlwm.call(['load', layout])
        x11.sync_with_hlwm()
        width1 = win1.get_geometry().width
        width2 = win2.get_geometry().width

        hlwm.attr.tags.focus.tiling.root[attr] = value
        assert hlwm.attr.tags.focus.tiling.root[attr]() == value
        assert hlwm.call('dump').stdout.strip() != layout

        x11.sync_with_hlwm()
        # both updated attributes must lead to an increas of width of
        # the first client:
        assert width1 < win1.get_geometry().width
        assert width2 != win2.get_geometry().width


def test_frame_split_fraction_invalid_arg(hlwm):
    hlwm.call(['load', '(split horizontal:0.5:0 (clients max:0) (clients max:0))'])

    # test values that clamp to 0.1
    for val in ['0.0', '0', '-0.1', '0.05', '0.1', '0.09', '-5', '-0.11']:
        # reset the fraction
        hlwm.attr.tags.focus.tiling.root.fraction = '0.5'
        assert hlwm.attr.tags.focus.tiling.root.fraction() == '0.5'

        hlwm.attr.tags.focus.tiling.root.fraction = val
        assert hlwm.attr.tags.focus.tiling.root.fraction() == '0.1'

    # test values that clamp to 0.9
    for val in ['12', '1', '0.9', '0.95', '0.9', '0.91', '12312', '+23']:
        # reset the fraction
        hlwm.attr.tags.focus.tiling.root.fraction = '0.87'
        assert hlwm.attr.tags.focus.tiling.root.fraction() == '0.87'

        hlwm.attr.tags.focus.tiling.root.fraction = val
        assert hlwm.attr.tags.focus.tiling.root.fraction() == '0.9'


def test_frame_leaf_algorithm_change(hlwm, x11):
    """
    Test that changing the layout algorithm affects window sizes
    """
    # two clients below each other
    win1, winid1 = x11.create_client()
    win2, winid2 = x11.create_client()
    hlwm.call('set_layout vertical')

    geom1_before = win1.get_geometry()
    geom2_before = win2.get_geometry()
    assert geom1_before.width == geom2_before.width
    assert geom1_before.height == geom2_before.height

    # put them side by side
    hlwm.attr.tags.focus.tiling.root.algorithm = 'horizontal'

    x11.sync_with_hlwm()
    geom1_now = win1.get_geometry()
    geom2_now = win2.get_geometry()
    # side by side implies: width decreases but height increases
    assert geom1_before.width > geom1_now.width
    assert geom1_before.height < geom1_now.height
    assert geom1_now.width == geom2_now.width
    assert geom1_now.height == geom2_now.height
