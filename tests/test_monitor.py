import pytest
from herbstluftwm.types import Rectangle


def test_default_monitor(hlwm):
    assert hlwm.get_attr('monitors.count') == '1'
    assert hlwm.get_attr('monitors.focus.name') == ''
    assert hlwm.get_attr('monitors.focus.index') == '0'


def test_add_monitor_requires_unfocused_tag(hlwm):
    hlwm.call_xfail('add_monitor 800x600+40+40 default monitor2')

    assert hlwm.get_attr('monitors.count') == '1'
    assert hlwm.get_attr('monitors.focus.name') == ''
    assert hlwm.get_attr('monitors.focus.index') == '0'


def test_add_monitor(hlwm):
    hlwm.call('add tag2')

    hlwm.call('add_monitor 800x600+40+40 tag2 monitor2')

    assert hlwm.get_attr('monitors.count') == '2'
    assert hlwm.get_attr('monitors.1.name') == 'monitor2'
    assert hlwm.attr.monitors[1].geometry() == '800x600+40+40'


def test_add_monitor_invalid_geometry(hlwm):
    hlwm.call('add tag2')

    hlwm.call_xfail('add_monitor 800x10+800+800 tag2') \
        .expect_stderr("Rectangle too small")

    hlwm.call_xfail('add_monitor 8x300+800+800 tag2') \
        .expect_stderr("Rectangle too small")

    # TODO: make 'rectangle' produce meaningful error messages
    hlwm.call_xfail('add_monitor not_a_rect_at_all tag2') \
        .expect_stderr("Rectangle too small")

    hlwm.call_xfail('add_monitor 1234 tag2') \
        .expect_stderr("Rectangle too small")

    hlwm.call_xfail('add_monitor 1234x tag2') \
        .expect_stderr("Rectangle too small")


def test_add_monitor_completion(hlwm):
    hlwm.call('add tagname')

    assert 'tagname' in hlwm.complete(['add_monitor', '800x600'])

    hlwm.command_has_all_args(['add_monitor', '800x600', 'tagname', 'monname'])


def test_add_monitor_with_no_name(hlwm):
    hlwm.call('add tag2')

    hlwm.call('add_monitor 800x600+40+40 tag2')

    assert hlwm.get_attr('monitors.count') == '2'
    assert hlwm.get_attr('monitors.1.name') == ''


def test_cannot_add_monitor_without_free_tag(hlwm):
    call = hlwm.call_xfail('add_monitor 800x600+40+40')
    assert call.stderr == 'add_monitor: There are not enough free tags\n'
    assert hlwm.get_attr('monitors.count') == '1'


def test_cannot_add_monitor_with_nonexistent_tag(hlwm):
    hlwm.call_xfail('add_monitor 800x600+40+40 derp') \
        .expect_stderr('no such tag: derp')
    assert hlwm.get_attr('monitors.count') == '1'


def test_cannot_add_monitor_with_already_viewed_tag(hlwm):
    hlwm.call('add tag2')
    call = hlwm.call_xfail('add_monitor 800x600+40+40 default')
    assert call.stderr == 'add_monitor: Tag "default" is already being viewed on a monitor\n'
    assert hlwm.get_attr('monitors.count') == '1'


def test_cannot_add_monitor_with_numeric_name(hlwm):
    hlwm.call('add tag2')
    call = hlwm.call_xfail('add_monitor 800x600+40+40 tag2 123foo')
    assert call.stderr == 'add_monitor: Invalid name "123foo": The monitor name may not start with a number\n'
    assert hlwm.get_attr('monitors.count') == '1'


def test_cannot_add_monitor_with_empty_name(hlwm):
    hlwm.call('add tag2')
    call = hlwm.call_xfail('add_monitor 800x600+40+40 tag2 ""')
    assert call.stderr == 'add_monitor: An empty monitor name is not permitted\n'
    assert hlwm.get_attr('monitors.count') == '1'


def test_cannot_add_monitor_with_existing_name(hlwm):
    hlwm.call('rename_monitor 0 mon1')
    hlwm.call('add tag2')
    call = hlwm.call_xfail('add_monitor 800x600+40+40 tag2 mon1')
    assert call.stderr == 'add_monitor: A monitor with the name "mon1" already exists\n'
    assert hlwm.get_attr('monitors.count') == '1'


def test_remove_monitor(hlwm):
    hlwm.call('add tag2')
    hlwm.call('add_monitor 800x600+40+40 tag2 monitor2')

    hlwm.call('remove_monitor 0')

    assert hlwm.get_attr('monitors.0.index') == '0'
    assert hlwm.get_attr('monitors.count') == '1'
    assert hlwm.get_attr('monitors.focus.name') == 'monitor2'


def test_remove_monitor_invalid_args(hlwm):
    hlwm.call_xfail('remove_monitor') \
        .expect_stderr('Expected one argument, but got only 0')

    assert hlwm.get_attr('monitors.count') == '1'

    hlwm.call('add tag2')
    hlwm.call('add_monitor 800x600+40+40 tag2 monitor2')

    hlwm.call_xfail('remove_monitor foobar') \
        .expect_stderr('No such monitor: foobar')

    hlwm.call_xfail('remove_monitor 2') \
        .expect_stderr('No such monitor: 2')

    assert hlwm.get_attr('monitors.count') == '2'


def test_cannot_remove_last_monitor(hlwm):
    call = hlwm.call_xfail('remove_monitor 0')
    assert call.stderr == 'remove_monitor: Can\'t remove the last monitor\n'
    assert hlwm.get_attr('monitors.count') == '1'


def test_remove_monitor_completion(hlwm):
    hlwm.call('add tag2')
    hlwm.call('add_monitor 800x600+40+40 tag2 monitor2')

    completions = hlwm.complete(['remove_monitor'])
    assert '0' in completions
    assert '1' in completions
    assert 'monitor2' in completions

    hlwm.command_has_all_args(['remove_monitor', '0'])


def test_move_monitor(hlwm):
    r = [8, 4, 400, 300]  # x,y,width,height
    hlwm.call('move_monitor \"0\" %dx%d%+d%+d' % (r[2], r[3], r[0], r[1]))
    assert hlwm.call('monitor_rect \"\"').stdout == ' '.join(map(str, r))


def test_move_monitor_completion(hlwm):
    assert hlwm.attr.monitors.focus.geometry() in hlwm.complete('move_monitor 0')
    # pads:
    assert '0' in hlwm.complete('move_monitor 0 800x600+0+0')
    assert '0' in hlwm.complete('move_monitor 0 800x600+0+0 0')
    assert '0' in hlwm.complete('move_monitor 0 800x600+0+0 0 0')
    assert '0' in hlwm.complete('move_monitor 0 800x600+0+0 0 0 0')

    hlwm.command_has_all_args(['move_monitor', '0', '800x600+0+0', '0', '0', '0', '0'])


def test_focus_monitor(hlwm):
    hlwm.call('add tag2')
    hlwm.call('add_monitor 800x600+40+40')
    assert hlwm.get_attr('monitors.focus.index') == '0'
    assert hlwm.get_attr('tags.focus.index') == '0'

    hlwm.call('focus_monitor 1')

    assert hlwm.get_attr('monitors.focus.index') == '1'
    assert hlwm.get_attr('tags.focus.index') == '1'


def test_focus_monitor_invalid_arg(hlwm):
    hlwm.call_xfail('focus_monitor foobar') \
        .expect_stderr('No such monitor: foobar')

    hlwm.call_xfail('focus_monitor 34') \
        .expect_stderr('No such monitor: 34')


def test_focus_monitor_shift_to_completion(hlwm):
    assert '0' in hlwm.complete('focus_monitor')
    hlwm.call('add othertag')
    hlwm.call('add_monitor 800x600+800+0 othertag monname')

    for cmd in ['focus_monitor', 'shift_to_monitor']:
        assert 'monname' in hlwm.complete(cmd)

        hlwm.command_has_all_args(['focus_monitor', '3'])
        hlwm.command_has_all_args(['focus_monitor', '3', 'dummy'])


def test_new_clients_appear_in_focused_monitor(hlwm):
    hlwm.call('add tag2')
    hlwm.call('add_monitor 800x600+40+40 tag2 monitor2')
    hlwm.call('focus_monitor monitor2')

    winid, _ = hlwm.create_client()

    assert hlwm.get_attr('tags.by-name.tag2.client_count') == '1'
    assert hlwm.get_attr('tags.by-name.default.client_count') == '0'
    assert hlwm.get_attr('clients', winid, 'tag') == 'tag2'


@pytest.mark.parametrize("arg", ['-l', '--list-all', '--no-disjoin'])
def test_detect_monitors_does_not_crash(hlwm, arg):
    # I don't know how to test detect_monitors properly, so just check that
    # it does not crash at least
    hlwm.call(['detect_monitors', arg])


def test_detect_monitors_affects_list_monitors(hlwm):
    list_monitors_before = hlwm.call('list_monitors').stdout
    hlwm.call('add othertag')
    hlwm.call('set_monitors 80x80+5+5 80x80+85+5')
    assert hlwm.get_attr('monitors.count') == '2'
    assert list_monitors_before != hlwm.call('list_monitors').stdout

    # check that detect monitors restores the one monitor
    hlwm.call('detect_monitors')

    assert hlwm.get_attr('monitors.count') == '1'
    assert list_monitors_before == hlwm.call('list_monitors').stdout


def test_rename_monitor(hlwm):
    hlwm.call('rename_monitor 0 foo')

    assert hlwm.get_attr('monitors.focus.name') == 'foo'
    assert hlwm.list_children('monitors.by-name') == ['foo']


def test_rename_monitor_no_name(hlwm):
    hlwm.call('rename_monitor 0 foo')

    hlwm.call('rename_monitor foo ""')

    assert hlwm.get_attr('monitors.focus.name') == ''
    assert hlwm.list_children('monitors.by-name') == []


def test_rename_monitor_invalid_arg(hlwm):
    hlwm.call('add othertag')
    hlwm.call('add_monitor 800x600+800+0 othertag othermon')
    hlwm.call_xfail('rename_monitor 0 othermon') \
        .expect_stderr('the name "othermon" already exists')


def test_rename_monitor_completion(hlwm):
    assert '0' in hlwm.complete('rename_monitor')
    hlwm.command_has_all_args(['rename_monitor', '0', 'foo'])


@pytest.mark.parametrize("tag_count", [1, 2, 3, 4])
@pytest.mark.parametrize("monitor_count", [1, 2, 3, 4])
def test_set_monitors_create_monitors(hlwm, tag_count, monitor_count):
    for i in range(1, tag_count):
        hlwm.call('add tag{}'.format(i))
    rects = ['100x200+{}+0'.format(100 * i) for i in range(0, monitor_count)]

    if tag_count < monitor_count:
        hlwm.call_xfail(['set_monitors'] + rects) \
            .expect_stderr('There are not enough free tags')
    else:
        hlwm.call(['set_monitors'] + rects)

        assert int(hlwm.get_attr('monitors.count')) == monitor_count
        monitors = hlwm.call('list_monitors').stdout.splitlines()
        assert rects == [s.split(' ')[1] for s in monitors]


@pytest.mark.parametrize("monitor_count_before", [1, 2, 3])
def test_set_monitors_removes_monitors(hlwm, monitor_count_before):
    for i in range(1, monitor_count_before):
        hlwm.call('add tag{}'.format(i))
        hlwm.call('add_monitor 100x200+{}+0'.format(100 * i))
    # focus the last monitor
    hlwm.call('focus_monitor {}'.format(monitor_count_before - 1))

    hlwm.call(['set_monitors', '100x200+100+0'])

    assert hlwm.get_attr('monitors.count') == '1'
    monitors = hlwm.call('list_monitors').stdout
    assert monitors == '0: 100x200+100+0 with tag "default" [FOCUS]\n'


def test_raise_monitor_completion(hlwm):
    hlwm.call('add tag2')
    hlwm.call('add_monitor 800x600+40+40 tag2 monitor2')

    expected = ['']
    expected += '-1 +0 +1 0 1 monitor2'.split(' ')
    expected.sort()
    assert hlwm.complete('raise_monitor', evaluate_escapes=True) == expected


def test_use_previous_on_tag_stealing_monitor(hlwm):
    hlwm.call('add tag2')
    hlwm.call('add tag3')
    hlwm.call('add_monitor 800x600+40+40 tag3 monitor2')

    hlwm.call('use tag2')
    hlwm.call('use tag3')  # steal it from monitor2
    hlwm.call('use_previous')

    assert hlwm.get_attr('tags.focus.name') == 'tag2'


def test_use_previous_on_stolen_monitor(hlwm):
    hlwm.call('add tag2')
    hlwm.call('add tag3')
    hlwm.call('add tag4')
    hlwm.call('add_monitor 800x600+40+40 tag4 monitor2')
    hlwm.call('focus_monitor monitor2')
    hlwm.call('use tag3')
    hlwm.call('focus_monitor 0')

    hlwm.call('use tag2')
    hlwm.call('use tag3')  # steal it from monitor2
    hlwm.call('focus_monitor monitor2')
    hlwm.call('use_previous')

    assert hlwm.get_attr('tags.focus.name') == 'tag3'


def test_use_previous_on_removed_tag(hlwm):
    hlwm.call('add rmtag')
    hlwm.call('add targettag')
    hlwm.call('use rmtag')
    hlwm.call('use_index 0')

    # 'targettag' should inherit everything from 'rmtag'
    hlwm.call('merge_tag rmtag targettag')
    hlwm.call('use_previous')

    assert hlwm.attr.tags.focus.name() == 'targettag'


def test_use_previous_nothing_viewed_before(hlwm):
    hlwm.call('add other_tag_that_may_not_be_used')

    # is a no-op:
    hlwm.call('use_previous')

    assert hlwm.attr.monitors.focus.index() == '0'


def test_use_previous_on_locked_monitor(hlwm):
    hlwm.call('add othertag')
    hlwm.call('use othertag')
    hlwm.call('use_index 0')

    hlwm.attr.monitors.focus.lock_tag = 'on'

    hlwm.call_xfail('use_previous') \
        .expect_stderr('Could not change tag.*monitor is locked')


def test_use_previous_no_completion(hlwm):
    hlwm.command_has_all_args(['use_previous'])


@pytest.mark.parametrize("two_monitors", [True, False])
def test_initial_client_position(hlwm, x11, two_monitors):
    # create two monitors side by side (with a little y-offset)
    if two_monitors:
        hlwm.call('add other')
        hlwm.call('set_monitors 173x174+4+5 199x198+200+100')
        hlwm.call('focus_monitor 1')
    hlwm.call('set_attr theme.border_width 0')  # disable border
    # add pad to the focused monitor and set its tag to floating
    hlwm.call('pad +0 12 13 14 15')
    hlwm.call('floating on')

    # create a new window in the area of the second monitor.
    g = (250, 160, 81, 82)  # x, y, width, height
    w, winid = x11.create_client(geometry=g)
    assert int(hlwm.get_attr('tags.focus.client_count')) == 1

    # check that the new client has the desired geometry
    win_geo = w.get_geometry()
    assert (win_geo.width, win_geo.height) == (g[2], g[3])
    x, y = x11.get_absolute_top_left(w)
    assert (x, y) == (g[0], g[1])


def test_shift_to_monitor(hlwm):
    hlwm.call('add tag2')
    hlwm.call('set_monitors 80x80+0+0 80x80+80+0')
    winid, _ = hlwm.create_client()
    oldtag = hlwm.get_attr('monitors.0.tag')
    assert hlwm.get_attr(f'clients.{winid}.tag') == oldtag

    hlwm.call('shift_to_monitor 1')

    newtag = hlwm.get_attr('monitors.1.tag')
    assert oldtag != newtag
    assert hlwm.get_attr(f'clients.{winid}.tag') == newtag


def test_shift_to_monitor_invalid_mon(hlwm):
    winid, _ = hlwm.create_client()
    hlwm.call_xfail('shift_to_monitor 34') \
        .expect_stderr('No such monitor: 34')


def test_shift_to_monitor_no_client(hlwm):
    hlwm.call('add tag2')
    hlwm.call('set_monitors 80x80+0+0 80x80+80+0')

    # there is no error message at the moment, so we only
    # check that it does not crash
    hlwm.call('shift_to_monitor 1')


def test_invalid_monitor_name(hlwm):
    cmds = [
        'list_padding', 'move_monitor',
        'rename_monitor', 'lock_tag', 'unlock_tag'
    ]
    for command in cmds:
        hlwm.call_xfail([command, 'thismonitordoesnotexist']) \
            .expect_stderr('Monitor "thismonitordoesnotexist" not found')


def test_list_padding(hlwm):
    hlwm.call('add othertag')
    hlwm.call('add_monitor 800x600+600+0')
    pad0 = '5 20 3 30'
    pad1 = '1 2 4 8'
    hlwm.call('pad 0 ' + pad0)
    hlwm.call('pad 1 ' + pad1)

    # this is a very primitive command, so we directly test multiple things at once
    assert hlwm.call('list_padding 0').stdout == pad0 + '\n'
    assert hlwm.call('list_padding 1').stdout == pad1 + '\n'

    assert hlwm.call('list_padding').stdout == pad0 + '\n'
    hlwm.call('focus_monitor 1')
    assert hlwm.call('list_padding').stdout == pad1 + '\n'


def test_list_padding_invalid_monitor(hlwm):
    hlwm.call_xfail('list_padding 23') \
        .expect_stderr('Monitor.*not found')


@pytest.mark.parametrize("mon_num,focus_idx", [
    (num, focus) for num in [1, 2, 3, 4, 5] for focus in [0, num - 1]])
@pytest.mark.parametrize("command, delta", [
    ('cycle_monitor', '-1'),
    ('cycle_monitor', '+1'),
    ('cycle_monitor', '1'),
    ('cycle_monitor', None),

    ('focus_monitor', '-1'),
    ('focus_monitor', '+1'),
])
def test_cycle_monitor(hlwm, mon_num, focus_idx, command, delta):
    """the present test also tests the MOD() function in utility.cpp"""
    for i in range(1, mon_num):
        hlwm.call('add tag' + str(i))
        hlwm.call('add_monitor 800x600+' + str(i * 10))
    hlwm.call(['focus_monitor', str(focus_idx)])
    assert hlwm.get_attr('monitors.focus.index') == str(focus_idx)
    assert hlwm.get_attr('monitors.count') == str(mon_num)

    if delta is None:
        hlwm.call([command])
        delta = 1  # For new_index calculation
    else:
        hlwm.call([command, delta])

    new_index = (focus_idx + int(delta) + mon_num) % mon_num
    assert hlwm.get_attr('monitors.focus.index') == str(new_index)


def test_cycle_monitor_invalid_arg(hlwm):
    hlwm.call_xfail(['cycle_monitor', '']) \
        .expect_stderr('stoi')

    hlwm.call_xfail(['cycle_monitor', 'mon']) \
        .expect_stderr('stoi')


def test_cycle_monitor_completion(hlwm):
    # just ensure that the monitor name does not show up in the completion
    hlwm.attr.monitors.focus.name = 'mainmonitor'

    res = hlwm.complete('cycle_monitor')
    assert res == sorted(['-1', '+1'])

    hlwm.command_has_all_args(['cycle_monitor', '+1'])


@pytest.mark.parametrize("lock_tag_cmd", [
    lambda index: ['lock_tag', str(index)],
    lambda index: ['set_attr', f'monitors.{index}.lock_tag', 'on']
])
def test_lock_tag_switch_away(hlwm, lock_tag_cmd):
    hlwm.call('add tag1')
    hlwm.call('add tag2')
    hlwm.call('set_monitors 800x600+0+0 800x600+800+0')
    hlwm.call(lock_tag_cmd(0))
    assert hlwm.attr.monitors[0].lock_tag() == hlwm.bool(True)

    hlwm.call('focus_monitor 0')

    # can not switch to another tag on the locked monitor
    hlwm.call_xfail('use tag2') \
        .expect_stderr('Could not change .*monitor 0 is locked')


@pytest.mark.parametrize("locked", [True, False])
def test_lock_tag_switch_to_locked(hlwm, locked):
    hlwm.call('add tag1')
    hlwm.call('add tag2')
    hlwm.call('set_monitors 800x600+0+0 800x600+800+0')
    hlwm.call('set swap_monitors_to_get_tag on')

    hlwm.attr.monitors[0].lock_tag = hlwm.bool(locked)
    hlwm.call('focus_monitor 1')

    # being on monitor 1, try to focus the tag on monitor 0
    tag_on_locked_monitor = hlwm.attr.monitors[0].tag()
    hlwm.call(['use', tag_on_locked_monitor])

    if locked:
        # if the monitor was locked, then the tag stays
        # on monitor 0
        expected_monitor_index = '0'
    else:
        # otherwise, the tag is moved to monitor 1 because
        # of swap_monitors_to_get_tag
        expected_monitor_index = '1'
    assert hlwm.attr.tags.focus.name() == tag_on_locked_monitor
    assert hlwm.attr.monitors.focus.index() == expected_monitor_index


def test_lock_tag_command_vs_attribute(hlwm):
    hlwm.call('add anothertag')
    hlwm.call('set_monitors 800x600+0+0 800x600+800+0')
    hlwm.call('focus_monitor 1')

    # no argument modifies the attribute of the focused monitor
    hlwm.call('lock_tag')
    assert hlwm.attr.monitors[1].lock_tag() == hlwm.bool(True)
    hlwm.call('unlock_tag')
    assert hlwm.attr.monitors[1].lock_tag() == hlwm.bool(False)

    hlwm.call('lock_tag 0')
    assert hlwm.attr.monitors[0].lock_tag() == hlwm.bool(True)
    hlwm.call('unlock_tag 0')
    assert hlwm.attr.monitors[0].lock_tag() == hlwm.bool(False)


def test_monitor_rect_too_small(hlwm):
    hlwm.call_xfail('attr monitors.0.geometry 20x300+0+0') \
        .expect_stderr('too small.*wide')
    hlwm.call_xfail('attr monitors.0.geometry 400x30+0+0') \
        .expect_stderr('too small.*high')


def test_monitor_rect_is_updated(hlwm):
    for rect in ['500x300+30+40', '500x300-30+40', '500x300+30-40', '500x300-30-40']:
        hlwm.call(['move_monitor', 0, rect])
        assert hlwm.attr.monitors[0].geometry() == rect


def test_monitor_rect_parser(hlwm):
    for rect in [(400, 800, a * 40, b * 60) for a in [1, -1] for b in [1, -1]]:

        hlwm.attr.monitors[0].geometry = '%dx%d%+d%+d' % rect

        expected_output = ' '.join([str(rect[i]) for i in [2, 3, 0, 1]])
        assert hlwm.call('monitor_rect 0').stdout.strip() == expected_output


def test_monitor_rect_apply_layout(hlwm, x11):
    winhandle, winid = x11.create_client()
    hlwm.attr.clients[winid].fullscreen = 'on'
    for expected_geometry in [(600, 700, 40, 50), (400, 800, -10, 0)]:
        hlwm.attr.monitors[0].geometry = '%dx%d%+d%+d' % expected_geometry

        x11.display.sync()
        geom = x11.get_absolute_geometry(winhandle)
        assert (geom.width, geom.height, geom.x, geom.y) == expected_geometry


def test_monitor_rect_command(hlwm):
    hlwm.call('add othertag')
    # two monitors with unique coordinates and pads
    hlwm.call('set_monitors 300x400+1+2 500x600+300+4')
    hlwm.attr.monitors[0].pad_left = 10
    hlwm.attr.monitors[0].pad_right = 20
    hlwm.attr.monitors[0].pad_up = 30
    hlwm.attr.monitors[0].pad_down = 40

    hlwm.attr.monitors[1].pad_left = 11
    hlwm.attr.monitors[1].pad_right = 21
    hlwm.attr.monitors[1].pad_up = 31
    hlwm.attr.monitors[1].pad_down = 41

    def monitor_rect(args):
        return hlwm.call(['monitor_rect'] + args).stdout.strip()

    assert monitor_rect([]) == '1 2 300 400'
    assert monitor_rect(['0']) == monitor_rect([])
    assert monitor_rect(['-p', '']) == '11 32 270 330'
    assert monitor_rect(['1']) == '300 4 500 600'
    assert monitor_rect(['-p', '1']) == '311 35 468 528'
    assert monitor_rect(['-p', '1']) == monitor_rect(['1', '-p'])


def test_monitor_rect_completion(hlwm):
    res = hlwm.complete('monitor_rect')
    assert '0' in res
    assert '-p' in res

    res = hlwm.complete('monitor_rect -p')
    assert '0' in res
    assert '-p' not in res

    res = hlwm.complete('monitor_rect 0')
    assert '0' not in res
    assert '-p' in res

    hlwm.command_has_all_args(['monitor_rect', '-p', '0'])
    hlwm.command_has_all_args(['monitor_rect', '0', '-p'])


def test_lock_unlock_sequence(hlwm):
    value = 0
    up = ('lock', 1)
    down = ('unlock', -1)
    assert int(hlwm.attr.settings.monitors_locked()) == value
    for cmd, delta in [up, up, down, up, down, down, down, down, up, up]:
        hlwm.call(cmd)
        value = max(0, value + delta)
        assert int(hlwm.attr.settings.monitors_locked()) == value


def test_lock_unlock_window_not_resized(hlwm, x11):
    hlwm.call(['load', '(split horizontal:0.5:1 (clients max:0) (clients max:0))'])
    win, _ = x11.create_client()
    hlwm.call('lock')

    width = win.get_geometry().width
    height = win.get_geometry().height

    hlwm.call(['resize', 'right'])
    x11.sync_with_hlwm()
    # monitors locked => geometry does not change
    assert width == win.get_geometry().width
    assert height == win.get_geometry().height

    hlwm.call(['unlock'])
    x11.sync_with_hlwm()
    # monitors unlocked => geometry is updated
    assert width != win.get_geometry().width
    assert height == win.get_geometry().height


def test_pad_command(hlwm):
    # parameter order of the pad command:
    pad_names = ['up', 'right', 'down', 'left']
    defaults = [10, 20, 30, 40]
    values = [11, 21, 31, 41]
    args_passed_so_far = set()
    # iterate over all combinations of omitting parameters
    # or passing empty parameters:
    for count, flags in [(count, flags)
                         for count in range(0, 5)
                         for flags in range(0, 2**count)]:
        # (re)set pad defaults
        for pad, val in zip(pad_names, defaults):
            hlwm.attr.monitors[0]['pad_' + pad] = val

        # we only pass parameters for 'count'-many elements of 'pad_names'
        # among these, only those are non-empty that are activated by 'flags',
        indices = [i for i in range(0, count) if ((2**i) & flags) != 0]
        args = [(values[i] if i in indices else '') for i in range(0, count)]
        hlwm.call(['pad', ''] + args)

        for idx, pad in enumerate(pad_names):
            expected = values[idx] if idx in indices else defaults[idx]
            assert int(hlwm.attr.monitors[0]['pad_' + pad]()) == expected

        # a little test-case self-test: verify that we really try distinct
        args = tuple(args)  # make it hashable
        assert args not in args_passed_so_far
        args_passed_so_far.add(args)

    assert len(args_passed_so_far) == 2**0 + 2**1 + 2**2 + 2**3 + 2**4


def test_pad_applied_to_floating_pos(hlwm):
    winid, _ = hlwm.create_client()
    clientobj = hlwm.attr.clients[winid]
    clientobj.floating = 'on'
    for monitor_rect in [Rectangle(width=300, height=400),
                         Rectangle(x=2, y=30, width=800, height=600)]:
        hlwm.attr.monitors.focus.geometry = monitor_rect.to_user_str()
        for left, up in [(10, 20), (5, 30)]:
            hlwm.attr.monitors.focus.pad_left = left
            hlwm.attr.monitors.focus.pad_up = up
            content_geom = Rectangle.from_user_str(clientobj.content_geometry())
            float_geom = Rectangle.from_user_str(clientobj.floating_geometry())

            assert content_geom.width == float_geom.width
            assert content_geom.height == float_geom.height
            assert content_geom.x == float_geom.x + left + monitor_rect.x
            assert content_geom.y == float_geom.y + up + monitor_rect.y


def test_pad_invalid_arg(hlwm):
    hlwm.call_xfail('pad') \
        .expect_stderr('Expected between 1 and 5')

    hlwm.call_xfail('pad 23') \
        .expect_stderr('No such monitor: 23')

    hlwm.call_xfail('pad 0 x') \
        .expect_stderr('stoi')

    hlwm.call_xfail('pad 0 23 x') \
        .expect_stderr('stoi')

    hlwm.call_xfail('pad 0 23 23 x') \
        .expect_stderr('stoi')

    hlwm.call_xfail('pad 0 23 23 23 x') \
        .expect_stderr('stoi')

    hlwm.call_xfail('pad 0 10 20 30 40 50') \
        .expect_stderr('Unknown argument or flag \"50\"')


def test_pad_completion(hlwm):
    assert '0' in hlwm.complete('pad')
    for cmd in ['pad 0', 'pad 0 12', 'pad 0 12 12', 'pad 0 12 12 12']:
        res = hlwm.complete(cmd, evaluate_escapes=True)
        assert '' in res
        assert '0' in res
    hlwm.command_has_all_args('pad 0 10 20 30 40'.split(' '))


def test_hide_covered_windows(hlwm, x11):
    hlwm.call('set_layout max')
    c1, winid1 = x11.create_client()
    c2, _ = x11.create_client()
    hlwm.call(['jumpto', winid1])

    all_values = [(v1, v2) for v1 in [False, True] for v2 in [False, True]]
    # try to catch all possible state changes
    for hide_covered, c1_pseudotiled in all_values + list(reversed(all_values)):
        hlwm.attr.settings.hide_covered_windows = hlwm.bool(hide_covered)
        hlwm.attr.clients[winid1].pseudotile = hlwm.bool(c1_pseudotiled)

        x11.sync_with_hlwm()
        geom1 = x11.get_absolute_geometry(c1)
        geom2 = x11.get_absolute_geometry(c2)

        # the size must always match
        if not c1_pseudotiled:
            assert geom1.width == geom2.width
            assert geom1.height == geom2.height
        # c1 must always be visible:
        assert geom1.x > 0
        assert geom1.y > 0

        if c1_pseudotiled or not hide_covered:
            # c2 visible:
            assert geom2.x > 0
            assert geom2.y > 0
        else:
            # c2 is not on the screen:
            assert geom2.x + geom2.width <= 0
            assert geom2.y + geom2.height <= 0
