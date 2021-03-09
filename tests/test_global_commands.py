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
