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


def test_new_clients_appear_in_focused_monitor(hlwm):
    hlwm.call('add tag2')
    hlwm.call('add_monitor 800x600+40+40 tag2 monitor2')
    hlwm.call('focus_monitor monitor2')

    hlwm.create_client()

    assert hlwm.get_attr('tags.by-name.tag2.client_count') == '1'
    assert hlwm.get_attr('tags.by-name.default.client_count') == '0'
    # TODO: Assert that client's winid is in tag2 (not yet possible)
