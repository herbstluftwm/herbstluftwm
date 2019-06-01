import pytest


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
    call = hlwm.call_xfail('add_monitor 800x600+40+40 derp')
    assert call.stderr == 'add_monitor: Tag "derp" does not exist\n'
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


def test_cannot_remove_nonexistent_monitor(hlwm):
    call = hlwm.call_xfail('remove_monitor 1')
    assert call.stderr == 'remove_monitor: Monitor "1" not found!\n'
    assert hlwm.get_attr('monitors.count') == '1'


def test_cannot_remove_last_monitor(hlwm):
    call = hlwm.call_xfail('remove_monitor 0')
    assert call.stderr == 'remove_monitor: Can\'t remove the last monitor\n'
    assert hlwm.get_attr('monitors.count') == '1'


def test_move_monitor(hlwm):
    r = [8, 4, 400, 300]  # x,y,width,height
    hlwm.call('move_monitor \"0\" %dx%d%+d%+d' % (r[2], r[3], r[0], r[1]))
    assert hlwm.call('monitor_rect \"\"').stdout == ' '.join(map(str, r))


def test_focus_monitor(hlwm):
    hlwm.call('add tag2')
    hlwm.call('add_monitor 800x600+40+40')
    assert hlwm.get_attr('monitors.focus.index') == '0'
    assert hlwm.get_attr('tags.focus.index') == '0'

    hlwm.call('focus_monitor 1')

    assert hlwm.get_attr('monitors.focus.index') == '1'
    assert hlwm.get_attr('tags.focus.index') == '1'


def test_new_clients_appear_in_focused_monitor(hlwm):
    hlwm.call('add tag2')
    hlwm.call('add_monitor 800x600+40+40 tag2 monitor2')
    hlwm.call('focus_monitor monitor2')

    winid, _ = hlwm.create_client()

    assert hlwm.get_attr('tags.by-name.tag2.client_count') == '1'
    assert hlwm.get_attr('tags.by-name.default.client_count') == '0'
    assert hlwm.get_attr('clients', winid, 'tag') == 'tag2'


def test_detect_monitors_does_not_crash(hlwm):
    # I don't know how to test detect_monitors properly, so just check that
    # it does not crash at least
    hlwm.call('detect_monitors')


def test_rename_monitor(hlwm):
    hlwm.call('rename_monitor 0 foo')

    assert hlwm.get_attr('monitors.focus.name') == 'foo'
    assert hlwm.list_children('monitors.by-name') == ['foo']


def test_rename_monitor_no_name(hlwm):
    hlwm.call('rename_monitor 0 foo')

    hlwm.call('rename_monitor foo ""')

    assert hlwm.get_attr('monitors.focus.name') == ''
    assert hlwm.list_children('monitors.by-name') == []


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
    assert hlwm.complete('raise_monitor') == expected


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
