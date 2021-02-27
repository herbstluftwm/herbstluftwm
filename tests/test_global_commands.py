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


def test_jumpto_bring_invalid_client(hlwm):
    for cmd in ['jumpto', 'bring']:
        hlwm.call_xfail([cmd, 'foobar']) \
            .expect_stderr('Cannot parse argument "foobar": stoul')

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
