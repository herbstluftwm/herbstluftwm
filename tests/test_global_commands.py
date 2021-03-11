import pytest


def test_tag_status_invalid_monitor(hlwm):
    hlwm.call_xfail('tag_status foobar') \
        .expect_stderr('No such monitor: foobar')


def test_tag_status_one_monitor(hlwm, x11):
    hlwm.call('add foobar')
    hlwm.call('add baz')
    hlwm.call('add qux')
    hlwm.create_client()
    hlwm.call('move baz')
    winid, _ = hlwm.create_client()
    hlwm.call('move qux')
    x11.make_window_urgent(x11.window(winid))

    assert hlwm.call('tag_status').stdout == "\t#default\t.foobar\t:baz\t!qux\t"


def test_tag_status_multi_monitor(hlwm):
    hlwm.call('chain , add othermon , add d1 , add d2 , add d3')
    hlwm.create_client()

    hlwm.call('add_monitor 800x600 othermon')
    hlwm.call('focus_monitor 1')

    assert hlwm.call('tag_status').stdout == hlwm.call('tag_status 1').stdout
    assert hlwm.call('tag_status 0').stdout == '\t+default\t%othermon\t.d1\t.d2\t.d3\t'
    assert hlwm.call('tag_status 1').stdout == '\t-default\t#othermon\t.d1\t.d2\t.d3\t'


def test_tag_status_completion(hlwm):
    monname = 'monitor_name'
    assert '0' in hlwm.complete('tag_status')
    assert '1' not in hlwm.complete('tag_status')
    assert monname not in hlwm.complete('tag_status')

    hlwm.call('add othertag')
    hlwm.call('add_monitor 800x600')

    assert '0' in hlwm.complete('tag_status')
    assert '1' in hlwm.complete('tag_status')
    assert monname not in hlwm.complete('tag_status')

    hlwm.attr.monitors[1].name = monname

    assert monname in hlwm.complete('tag_status')

    hlwm.command_has_all_args(['tag_status', '0'])


def test_jumpto_bring_invalid_client(hlwm):
    for cmd in ['jumpto', 'bring']:
        hlwm.call_xfail([cmd, 'foobar']) \
            .expect_stderr('Cannot parse argument "foobar": Invalid format, expecting')

        hlwm.call_xfail([cmd, 'urgent']) \
            .expect_stderr('No client is urgent')

        hlwm.call_xfail([cmd, '']) \
            .expect_stderr('No client is focused')

        hlwm.call_xfail([cmd, '0xdead']) \
            .expect_stderr('No managed client with window id 0xdead')


def test_jumpto_bring_completion(hlwm):
    for cmd in ['jumpto', 'bring']:
        winid, proc = hlwm.create_client()

        res = hlwm.complete([cmd])
        assert winid in res
        assert 'urgent' in res

        proc.kill()
        proc.wait(10)

        res = hlwm.complete([cmd])
        assert winid not in res
        assert 'urgent' in res

        hlwm.command_has_all_args([cmd, 'urgent'])
        hlwm.command_has_all_args([cmd, ''])


def test_cycle_value_color(hlwm):
    values = ['#ff0000', '#00ff00', '#0000ff']
    # try the color test both via settings and via attributes
    for prefix in ['', 'settings.']:
        # this setting is a color, and even a DynAttribute
        name = 'window_border_active_color'
        hlwm.call(f'set {name} orange')

        for i in range(0, 5):
            hlwm.call(['cycle_value', prefix + name] + values)
            assert hlwm.get_attr('settings.' + name) == values[i % len(values)]


def test_cycle_value_loop(hlwm):
    values = ['0', '1', '2', '2', '3', '4']
    name = 'frame_gap'
    hlwm.call(f'set {name} 3')
    # if we now run cycle_value multiple times, it should reach the 4
    # and in the next loop should hang at the 2
    expected_values = ['4', '0', '1', '2', '2', '2', '2', '2']
    for expected in expected_values:
        hlwm.call(['cycle_value', name] + values)
        assert hlwm.get_attr('settings.' + name) == expected


def test_cycle_value_completion(hlwm):
    assert 'tags.0.name ' in hlwm.complete(['cycle_value', 'tags.0.'], partial=True, position=1)

    assert 'monitors.focus.' in hlwm.complete(['cycle_value', 'monitors.'], partial=True, position=1)
    # 'monitors.count' is however not completed because it is read only:
    assert [] == hlwm.complete(['cycle_value', 'monitors.cou'], partial=True, position=1)

    # the completion depends on the attribute type:
    assert 'true' in hlwm.complete(['cycle_value', 'settings.always_show_frame'])
    assert 'vertical' in hlwm.complete(['cycle_value', 'settings.default_frame_layout'])
    assert 'vertical' in hlwm.complete(['cycle_value', 'tags.focus.tiling.focused_frame.algorithm'])


def test_cycle_value_invalid_arg(hlwm):
    hlwm.call_xfail('cycle_value foobar baz') \
        .expect_stderr('No such attribute: foobar\n')

    hlwm.call('new_attr int clients.my_foo 1')
    command = 'cycle_value clients.my_foo 1 2 bar'

    # calling it a first time works
    hlwm.call(command)
    assert hlwm.attr.clients.my_foo() == '2'

    # calling it a second time fails
    hlwm.call_xfail(command) \
        .expect_stderr('Invalid value for "clients.my_foo": ')


def test_use_tag(hlwm):
    hlwm.call('add foo')
    assert hlwm.attr.tags.focus.name() == 'default'

    hlwm.call('use foo')

    assert hlwm.attr.tags.focus.name() == 'foo'


def test_use_tag_completion(hlwm):
    hlwm.call('add foo')

    assert 'foo' in hlwm.complete(['use'])

    hlwm.call('add bar')
    res = hlwm.complete(['use'])
    assert 'foo' in res
    assert 'bar' in res

    hlwm.command_has_all_args(['use', 'foo'])


def test_use_index_completion(hlwm):
    assert hlwm.complete(['use_index']) == sorted(['+1', '-1', '--skip-visible'])
    assert hlwm.complete(['use_index', '+1']) == sorted(['--skip-visible'])
    assert hlwm.complete(['use_index', '--skip-visible']) == sorted(['+1', '-1'])
    hlwm.command_has_all_args(['use_index', '--skip-visible', '0'])


def test_use_tag_invalid_arg(hlwm):
    hlwm.call_xfail('use foobar') \
        .expect_stderr('no such tag: foobar')


def test_use_tag_use_index_monitor_locked(hlwm):
    hlwm.call('add foo')
    hlwm.attr.monitors.focus.lock_tag = 'on'

    for cmd in ['use foo', 'use_index +1', 'use_index 1']:
        hlwm.call_xfail(cmd) \
            .expect_stderr('Could not change.*is locked')


def test_use_index_invalid_arg(hlwm):
    for arg in ['+f', '34', '--', '+', '-', '+3f', '-x', 'foo']:
        hlwm.call_xfail(['use_index', arg]) \
            .expect_stderr(f'Invalid index "{arg}"', regex=False)


@pytest.mark.parametrize("delta", ['+1', '-1'])
@pytest.mark.parametrize("skip_visible", [True, False])
def test_use_index_skip_visible(hlwm, delta, skip_visible):
    # create the tags in a order such that (0 + delta) (modulo tag
    # count) is the used tag:
    if delta == '+1':
        hlwm.call('chain , add used , add free , add dummy')
    else:
        hlwm.call('chain , add dummy , add free , add used')

    hlwm.call('add_monitor 800x600+800+0 used')
    assert hlwm.attr.monitors.focus.index() == '0'
    assert hlwm.attr.tags.focus.index() == '0'

    cmd = ['use_index', delta]

    if skip_visible:
        cmd += ['--skip-visible']
        # --skip-visible will skip the 'used' tag and
        # go to the next free tag
        expected_name = 'free'
    else:
        # without, it will steal the 'used' tag
        expected_name = 'used'

    hlwm.call(cmd)

    assert hlwm.attr.monitors.focus.index() == '0'
    assert hlwm.attr.tags.focus.name() == expected_name
